from pathlib import Path
import re
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
CANDIDATES = (Path.cwd(), SCRIPT_DIR, SCRIPT_DIR.parent)
ROOT = next((candidate for candidate in CANDIDATES if (candidate / "frontend" / "server.py").exists()), Path.cwd())
SERVER = ROOT / "frontend" / "server.py"

if not SERVER.exists():
    sys.exit("Error: ejecuta este script en la raíz del repositorio; no se encontró frontend/server.py.")

source = SERVER.read_text(encoding="utf-8")
original = source

# 1) Parse public Origin/Host values correctly behind Render's HTTPS proxy.
if "from urllib.parse import urlsplit" not in source:
    source = source.replace("import time\n", "import time\nfrom urllib.parse import urlsplit\n", 1)

old_host_method = '''    def host_allowed(self):
        port = self.server.server_port
        return self.headers.get("Host") in {f"127.0.0.1:{port}", f"localhost:{port}"}
'''
new_host_method = '''    def host_allowed(self):
        host = self.headers.get("Host", "")
        if not host or any(char.isspace() for char in host) or "/" in host or "\\\\" in host:
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
'''
if old_host_method in source:
    source = source.replace(old_host_method, new_host_method, 1)
elif "def origin_matches_host(self, origin):" not in source:
    sys.exit("Error: no se pudo localizar el método host_allowed(); no se modificó server.py.")

# 2) Add a lightweight HTTP health endpoint for Render.
old_get = '''    def do_GET(self):
        if not self.host_allowed():
            self.send_error(403)
        elif self.path in ASSETS:
'''
new_get = '''    def do_GET(self):
        if not self.host_allowed():
            self.send_error(403)
        elif self.path == "/healthz":
            self.send_json({"status": "ok"})
        elif self.path in ASSETS:
'''
if old_get in source:
    source = source.replace(old_get, new_get, 1)
elif 'self.path == "/healthz"' not in source:
    sys.exit("Error: no se pudo agregar /healthz; no se modificó server.py.")

# 3) Match Origin by hostname/port instead of assuming local HTTP.
old_post_guard = '''        if (not self.host_allowed() or (origin and origin != "http://" + self.headers["Host"])
                or self.headers.get_content_type() != "application/json"):
'''
new_post_guard = '''        if (not self.host_allowed() or (origin and not self.origin_matches_host(origin))
                or self.headers.get_content_type() != "application/json"):
'''
if old_post_guard in source:
    source = source.replace(old_post_guard, new_post_guard, 1)
elif "not self.origin_matches_host(origin)" not in source:
    sys.exit("Error: no se pudo adaptar la validación de Origin; no se modificó server.py.")

# 4) Never run arbitrary user-generated executables in the public demo service.
post_parse_marker = '''        except (ValueError, RecursionError) as error:
            self.send_json({"error": str(error)}, 400)
            return
'''
execution_guard = post_parse_marker + '''        allow_execution = os.environ.get("ALLOW_CODE_EXECUTION", "true").strip().lower() in {"1", "true", "yes"}
        if options[2] and not allow_execution:
            self.send_json({"error": "La ejecución de programas está deshabilitada en la demo pública. Puedes compilar y revisar el AST, IR y ensamblador."}, 403)
            return
'''
if post_parse_marker in source and "allow_execution = os.environ.get" not in source:
    source = source.replace(post_parse_marker, execution_guard, 1)

# 5) Bind to all interfaces and use Render's PORT. Use the image's prebuilt bridge if present.
main_start = source.find("def main():\n")
main_end = source.find("\n\nif __name__ == \"__main__\":", main_start)
if main_start < 0 or main_end < 0:
    sys.exit("Error: no se pudo localizar main(); no se modificó server.py.")
new_main = '''def serve(port, binary):
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
        parser.exit(1, "Inicia este servidor en Ubuntu o WSL x86-64.\\n")
    if any(shutil.which(tool) is None for tool in ("g++", "gcc", "prlimit")):
        parser.exit(1, "Instala g++, gcc y util-linux en Ubuntu o WSL.\\n")

    configured_bridge = os.environ.get("COMPILER_BRIDGE")
    if configured_bridge:
        binary = Path(configured_bridge)
        if not binary.is_file() or not os.access(binary, os.X_OK):
            parser.exit(1, "No se encontró el compilador puente precompilado.\\n")
        serve(args.port, binary)
        return

    with tempfile.TemporaryDirectory(prefix="compilador-web-") as directory:
        binary = Path(directory) / "bridge"
        print("Preparando el compilador…", flush=True)
        build_bridge(binary)
        serve(args.port, binary)
'''
source = source[:main_start] + new_main + source[main_end:]

if source == original:
    print("server.py ya parece estar configurado para Render; no se realizaron cambios.")
else:
    SERVER.write_text(source, encoding="utf-8", newline="\n")
    print("Actualizado: frontend/server.py")

# Docker image: compile the bridge during image build, not on every cold start.
dockerfile = '''FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \\
    PYTHONDONTWRITEBYTECODE=1 \\
    PYTHONUNBUFFERED=1 \\
    PORT=10000 \\
    ALLOW_CODE_EXECUTION=false \\
    COMPILER_BRIDGE=/usr/local/bin/compiler-bridge

RUN apt-get update \\
    && apt-get install -y --no-install-recommends g++ gcc python3 util-linux ca-certificates \\
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

RUN set -eux; \\
    SOURCES="$(find /app -maxdepth 1 -type f -name '*.cpp' ! -name 'main.cpp' | sort)"; \\
    g++ -std=c++17 -O1 -I /app /app/frontend/bridge.cpp $SOURCES -o /usr/local/bin/compiler-bridge

RUN useradd --system --uid 10001 --home-dir /nonexistent --shell /usr/sbin/nologin appuser
USER appuser

EXPOSE 10000
CMD ["python3", "frontend/server.py"]
'''
(ROOT / "Dockerfile").write_text(dockerfile, encoding="utf-8", newline="\n")

ignore = '''.git
.github
__pycache__/
**/__pycache__/
*.py[cod]
.env
.env.*
*.log
outputs/*
!outputs/.gitkeep
.DS_Store
Thumbs.db
'''
(ROOT / ".dockerignore").write_text(ignore, encoding="utf-8", newline="\n")
print("Creados: Dockerfile y .dockerignore")
print("Modo público: la compilación y visualización están disponibles; la ejecución de binarios permanece deshabilitada por seguridad.")
'''
(ROOT / "DEPLOY_RENDER.md").write_text('''# Despliegue en Render\n\n## Preparar cambios\n\nDesde la raíz del repositorio, ejecutar:\n\n```bash\npython render_setup/apply_render_setup.py\n```\n\nEl script adapta el servidor para Render, agrega `/healthz`, prepara el `Dockerfile` y crea `.dockerignore`.\n\n## Subir configuración\n\n```bash\ngit add frontend/server.py Dockerfile .dockerignore DEPLOY_RENDER.md\ngit commit -m "Prepare compiler app for Render deployment"\ngit push -u origin deploy/render\n```\n\n## Crear servicio en Render\n\n- Tipo: Web Service\n- Workspace: My Workspace\n- Runtime: Docker\n- Dockerfile: `./Dockerfile`\n- Rama: `deploy/render`\n- Plan: Free\n- Región: Oregon\n- Health check path: `/healthz`\n\nEl Dockerfile compila el puente C++ durante la construcción de la imagen y el servidor escucha en `0.0.0.0:$PORT`.\n\n## Limitación de seguridad de la demo pública\n\n`ALLOW_CODE_EXECUTION=false` impide ejecutar los binarios creados a partir del código enviado por visitantes. La interfaz permite compilar y revisar AST, IR y ensamblador. No habilitar ejecución pública hasta aislarla en una sandbox independiente con límites y políticas de red apropiadas.\n\nEl servicio gratuito de Render puede suspenderse tras un periodo sin tráfico, por lo que la primera petición posterior puede tardar en responder.\n''', encoding="utf-8", newline="\n")
