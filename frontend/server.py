"""Interfaz local del compilador para Linux x86-64 (incluye Ubuntu en WSL)."""

import argparse
import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import tempfile
import threading
import time
from urllib.parse import urlsplit

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
MAX_SOURCE = 64 * 1024
MAX_REQUEST = 512 * 1024
ASSETS = {
    "/": ("index.html", "text/html"),
    "/styles.css": ("styles.css", "text/css"),
    "/app.js": ("app.js", "text/javascript"),
    "/ast.js": ("ast.js", "text/javascript"),
}
JOBS = threading.BoundedSemaphore(2)


def build_bridge(binary):
    sources = sorted(path for path in ROOT.glob("*.cpp") if path.name != "main.cpp")
    subprocess.run(
        ["g++", "-std=c++17", "-O1", "-I", str(ROOT), str(HERE / "bridge.cpp"),
         *map(str, sources), "-o", str(binary)],
        check=True,
    )


def read_text(path):
    return path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""


def diagnostic(phase, message, severity="error", line=None, column=None):
    return dict(phase=phase, message=message, severity=severity, line=line, column=column)


def with_location(item, source_bytes):
    offset = item.pop("offset")
    line = column = None
    if 0 <= offset <= len(source_bytes):
        prefix = source_bytes[:offset].decode("utf-8", errors="replace")
        line = prefix.count("\n") + 1
        # El editor usa posiciones UTF-16, como selectionStart de un textarea.
        column = len(prefix.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2 + 1
    return diagnostic(item["phase"], item["message"], line=line, column=column)


def run_process(command, folder, timeout, max_file_size, label):
    stdout_path = folder / (label + ".stdout")
    stderr_path = folder / (label + ".stderr")
    limited = ["prlimit", "--core=0", "--as=536870912", "--fsize=" + str(max_file_size),
               "--", *map(str, command)]
    started = time.perf_counter()
    timed_out = False
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        process = subprocess.Popen(limited, cwd=folder, stdin=subprocess.DEVNULL,
                                   stdout=stdout, stderr=stderr, start_new_session=True)
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()

    return dict(
        stdout=read_text(stdout_path),
        stderr=read_text(stderr_path),
        exitCode=None if timed_out or process.returncode < 0 else process.returncode,
        signal=None if timed_out or process.returncode >= 0 else -process.returncode,
        timedOut=timed_out,
        elapsedMs=round((time.perf_counter() - started) * 1000, 1),
    )


def failure(result, fallback):
    if result["timedOut"]:
        return "Se agotó el tiempo disponible. " + fallback
    if result["signal"] == signal.SIGXFSZ:
        return "Se superó el límite de tamaño de salida."
    return result["stderr"].strip() or fallback


def compile_source(bridge, source, optimize, security, execute):
    source_bytes = source.encode("utf-8")
    if len(source_bytes) > MAX_SOURCE or b"\0" in source_bytes:
        raise ValueError("El código debe tener hasta 64 KiB y no contener caracteres nulos.")
    warning_prefix = "Advertencia de seguridad: "
    with tempfile.TemporaryDirectory(prefix="compilador-") as directory:
        folder = Path(directory)
        (folder / "source.cpp").write_bytes(source_bytes)
        build = run_process([bridge, int(optimize), int(security)], folder, 6, 2 * 1024 * 1024, "bridge")
        diagnostics = [
            diagnostic("seguridad", line[len(warning_prefix):], "warning")
            for line in build["stdout"].splitlines() if line.startswith(warning_prefix)
        ]
        result = dict(
            ok=build["exitCode"] == 0,
            compileMs=build["elapsedMs"],
            diagnostics=diagnostics,
            ast=None,
            irBefore=read_text(folder / "before.ir"),
            irAfter=read_text(folder / "after.ir"),
            assembly=read_text(folder / "program.s"),
            execution=None,
        )
        ast = read_text(folder / "ast.json")
        if ast:
            try:
                result["ast"] = json.loads(ast)
            except (json.JSONDecodeError, RecursionError):
                if result["ok"]:
                    diagnostics.append(diagnostic("AST", "El árbol supera la profundidad que admite el visor.", "warning"))
        if not result["ok"]:
            result["assembly"] = ""
            raw = read_text(folder / "diagnostic.json")
            if raw:
                diagnostics.append(with_location(json.loads(raw), source_bytes))
            else:
                diagnostics.append(diagnostic("compilación", failure(build, "El compilador se detuvo.")))
            return result
        if not execute:
            return result

        link = run_process(["gcc", "program.s", "-o", "program"], folder, 5, 2 * 1024 * 1024, "gcc")
        if link["exitCode"] != 0:
            diagnostics.append(diagnostic("ensamblado", failure(link, "GCC no pudo crear el ejecutable.")))
            result["ok"] = False
            return result

        execution = run_process([folder / "program"], folder, 2, 128 * 1024, "program")
        result["execution"] = execution
        if execution["timedOut"]:
            diagnostics.append(diagnostic("ejecución", "Se detuvo el programa después de 2 segundos."))
        elif execution["signal"] == signal.SIGXFSZ:
            diagnostics.append(diagnostic("ejecución", "Se alcanzó el límite de salida de 128 KiB."))
        elif execution["signal"]:
            diagnostics.append(diagnostic("ejecución", f"El programa terminó por la señal {execution['signal']}."))
        elif execution["stderr"].startswith("Error de ejecucion:"):
            diagnostics.append(diagnostic("seguridad", execution["stderr"].strip()))
        return result


class Handler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(10)

    def send_content(self, content, content_type, status=200):
        self.send_response(status)
        self.send_header("Content-Type", f"{content_type}; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy",
                         "default-src 'self'; base-uri 'none'; frame-ancestors 'none'; object-src 'none'")
        self.end_headers()
        self.wfile.write(content)

    def send_json(self, data, status=200):
        self.send_content(json.dumps(data, ensure_ascii=False).encode("utf-8"), "application/json", status)

    def host_allowed(self):
        host = self.headers.get("Host", "")
        if not host or any(char.isspace() for char in host) or "/" in host or "\\" in host:
            return False
        try:
            return urlsplit("//" + host).hostname is not None
        except ValueError:
            return False

    def origin_matches_host(self, origin):
        try:
            parsed_origin = urlsplit(origin)
            parsed_host = urlsplit("//" + self.headers.get("Host", ""))
            if (parsed_origin.scheme not in {"http", "https"}
                    or not parsed_origin.hostname or not parsed_host.hostname
                    or parsed_origin.hostname.lower() != parsed_host.hostname.lower()):
                return False
            origin_port = parsed_origin.port or (443 if parsed_origin.scheme == "https" else 80)
            host_port = parsed_host.port or origin_port
            return origin_port == host_port
        except ValueError:
            return False

    def do_GET(self):
        if not self.host_allowed():
            self.send_error(403)
        elif self.path == "/healthz":
            self.send_json({"status": "ok"})
        elif self.path in ASSETS:
            name, content_type = ASSETS[self.path]
            self.send_content((HERE / name).read_bytes(), content_type)
        elif self.path == "/api/examples":
            examples = sorted((ROOT / "inputs").glob("input*.txt"), key=lambda p: int(p.stem[5:])
                              if p.stem[5:].isdigit() else float("inf"))
            self.send_json([{"name": p.name, "source": read_text(p)} for p in examples])
        else:
            self.send_error(404)

    def do_POST(self):
        origin = self.headers.get("Origin")
        if (not self.host_allowed() or (origin and not self.origin_matches_host(origin))
                or self.headers.get_content_type() != "application/json"):
            self.send_error(403)
            return
        if self.path != "/api/compile":
            self.send_error(404)
            return
        try:
            length = int(self.headers.get("Content-Length", 0))
            if not 0 < length <= MAX_REQUEST:
                raise ValueError("La solicitud está vacía o es demasiado grande.")
            data = json.loads(self.rfile.read(length))
            if not isinstance(data, dict) or not isinstance(data.get("source"), str):
                raise ValueError("Falta el código fuente.")
            options = [data.get(key, default) for key, default in
                       [("optimize", True), ("security", True), ("execute", False)]]
            if any(type(value) is not bool for value in options):
                raise ValueError("Las opciones deben ser valores booleanos.")
        except (ValueError, RecursionError) as error:
            self.send_json({"error": str(error)}, 400)
            return
        allow_execution = os.environ.get("ALLOW_CODE_EXECUTION", "true").strip().lower() in {"1", "true", "yes"}
        if options[2] and not allow_execution:
            self.send_json({"error": "La ejecución de programas está deshabilitada en la demo pública. Puedes compilar y revisar el AST, IR y ensamblador."}, 403)
            return
        if not JOBS.acquire(blocking=False):
            self.send_json({"error": "Hay dos compilaciones en curso. Inténtalo de nuevo."}, 429)
            return
        try:
            result = compile_source(self.server.bridge, data["source"], *options)
        except ValueError as error:
            self.send_json({"error": str(error)}, 400)
            return
        finally:
            JOBS.release()
        self.send_json(result)


def serve(port, binary):
    with ThreadingHTTPServer(("0.0.0.0", port), Handler) as server:
        server.bridge = binary
        print(f"Servidor listo en el puerto {server.server_port}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=int(os.environ.get("PORT", "8765")))
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        parser.exit(1, "Inicia este servidor en Ubuntu o WSL x86-64.\n")
    if any(shutil.which(tool) is None for tool in ("g++", "gcc", "prlimit")):
        parser.exit(1, "Instala g++, gcc y util-linux en Ubuntu o WSL.\n")

    configured_bridge = os.environ.get("COMPILER_BRIDGE")
    if configured_bridge:
        binary = Path(configured_bridge)
        if not binary.is_file() or not os.access(binary, os.X_OK):
            parser.exit(1, "No se encontró el compilador puente precompilado.\n")
        serve(args.port, binary)
        return

    with tempfile.TemporaryDirectory(prefix="compilador-web-") as directory:
        binary = Path(directory) / "bridge"
        print("Preparando el compilador…", flush=True)
        build_bridge(binary)
        serve(args.port, binary)


if __name__ == "__main__":
    main()

