from pathlib import Path

ROOT = Path(__file__).resolve().parent
# The installer is expected to be copied into the repository root.
APP = ROOT / "frontend" / "app.js"
SERVER = ROOT / "frontend" / "server.py"


def replace_once(content: str, old: str, new: str, label: str) -> str:
    count = content.count(old)
    if count != 1:
        raise SystemExit(
            f"No se pudo aplicar {label}: se esperaba encontrar exactamente una coincidencia; encontradas: {count}."
        )
    return content.replace(old, new, 1)


def main() -> None:
    if not APP.is_file() or not SERVER.is_file():
        raise SystemExit(
            "Ejecuta este script desde la raíz de Proyecto2_Compiladores; "
            "deben existir frontend/app.js y frontend/server.py."
        )

    app = APP.read_text(encoding="utf-8")
    server = SERVER.read_text(encoding="utf-8")

    helper_anchor = "async function submit(execute) {"
    helper = r'''const WANDBOX_API_URL = "https://wandbox.org/api/compile.json";
const WANDBOX_COMPILER = "gcc-head";

function makeWandboxSource(assembly) {
  if (typeof assembly !== "string" || !assembly.trim()) {
    throw new Error("El compilador no generó ensamblador para ejecutar.");
  }

  // C++ raw strings preserve GAS syntax (quotes, backslashes, %, $, and newlines).
  // Pick a delimiter that cannot accidentally terminate inside user-generated assembly.
  let delimiter = "WASM";
  let suffix = 0;
  while (assembly.includes(`)${delimiter}"`)) {
    suffix += 1;
    delimiter = `WASM${suffix}`;
    if (delimiter.length > 16) {
      throw new Error("No se pudo preparar el ensamblador para Wandbox.");
    }
  }

  return `__asm__(R"${delimiter}(\n${assembly}\n)${delimiter}");\n`;
}

async function runAssemblyOnWandbox(assembly) {
  const started = performance.now();
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 30000);

  const payload = {
    compiler: WANDBOX_COMPILER,
    code: makeWandboxSource(assembly),
    "compiler-option-raw": "-std=gnu++17\n-O0",
    "runtime-option-raw": "",
    stdin: "",
    save: false,
  };

  try {
    const response = await fetch(WANDBOX_API_URL, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      signal: controller.signal,
    });

    const responseText = await response.text();
    let data;
    try {
      data = JSON.parse(responseText);
    } catch {
      throw new Error(`Wandbox devolvió una respuesta no válida (HTTP ${response.status}).`);
    }

    if (!response.ok) {
      const detail = data.error || data.message || responseText.slice(0, 300);
      throw new Error(`Wandbox respondió HTTP ${response.status}: ${detail}`);
    }

    const compilerError = String(data.compiler_error || "").trim();
    const rawStatus = data.status;
    const exitCode = rawStatus === undefined || rawStatus === null || rawStatus === "" || compilerError
      ? null
      : Number(rawStatus);

    if (exitCode !== null && !Number.isFinite(exitCode)) {
      throw new Error("Wandbox devolvió un código de salida inesperado.");
    }

    const execution = {
      stdout: String(data.program_output || ""),
      stderr: String(data.program_error || compilerError || ""),
      exitCode,
      signal: data.signal || null,
      timedOut: false,
      elapsedMs: Math.round((performance.now() - started) * 10) / 10,
    };

    return { execution, compilerError };
  } catch (error) {
    if (error && error.name === "AbortError") {
      throw new Error("Wandbox tardó demasiado en responder (límite local: 30 segundos). Inténtalo nuevamente.");
    }
    throw error;
  } finally {
    clearTimeout(timeout);
  }
}

'''

    if "runAssemblyOnWandbox" not in app:
        app = replace_once(app, helper_anchor, helper + helper_anchor, "helper de Wandbox")
    elif "const WANDBOX_API_URL" not in app:
        raise SystemExit("app.js ya contiene una referencia a Wandbox incompleta; revisa el archivo antes de continuar.")

    old_body = 'body: JSON.stringify({ source: source.value, optimize: optimize.checked, security: security.checked, execute }),'
    new_body = 'body: JSON.stringify({ source: source.value, optimize: optimize.checked, security: security.checked, execute: false }),' 
    if old_body in app:
        app = replace_once(app, old_body, new_body, "desactivar ejecución local del servidor")
    elif new_body not in app:
        raise SystemExit("No se encontró el cuerpo esperado de /api/compile en app.js.")

    result_anchor = '    if (!response.ok) throw new Error(result.error || "La solicitud falló.");\n    if (submitted !== revision) return;'
    result_replacement = '''    if (!response.ok) throw new Error(result.error || "La solicitud falló.");
    if (submitted !== revision) return;

    if (execute && result.ok) {
      setStatus("Enviando ensamblador a Wandbox…", "ready");
      const remote = await runAssemblyOnWandbox(result.assembly);
      result.execution = remote.execution;
      if (remote.compilerError) {
        const isError = /(error:|fatal error:|undefined reference|ld returned|cannot find)/i.test(remote.compilerError);
        result.diagnostics.push({
          phase: "ensamblado",
          message: remote.compilerError,
          severity: isError ? "error" : "warning",
          line: null,
          column: null,
        });
      }
    }
'''
    if result_anchor in app:
        app = replace_once(app, result_anchor, result_replacement, "invocación remota de Wandbox")
    elif "result.execution = remote.execution" not in app:
        raise SystemExit("No se encontró el punto esperado para insertar la ejecución remota.")

    old_csp = "default-src 'self'; base-uri 'none'; frame-ancestors 'none'; object-src 'none'"
    new_csp = "default-src 'self'; connect-src 'self' https://wandbox.org; base-uri 'none'; frame-ancestors 'none'; object-src 'none'"
    if old_csp in server:
        server = replace_once(server, old_csp, new_csp, "CSP para permitir la API de Wandbox")
    elif new_csp not in server:
        raise SystemExit("No se encontró la política CSP esperada en frontend/server.py.")

    APP.write_text(app, encoding="utf-8", newline="\n")
    SERVER.write_text(server, encoding="utf-8", newline="\n")
    print("Integración de Wandbox aplicada correctamente.")
    print("- Ejecutar envía el ensamblador GAS/AT&T generado a Wandbox (GCC + GNU assembler).")
    print("- El backend local solo compila el lenguaje del proyecto; no ejecuta el binario.")
    print("- La CSP permite conexiones desde el navegador hacia https://wandbox.org.")
    print("- Límite de espera del navegador: 30 segundos.")
    print("Revisa el diff, prueba ejemplos y luego haz commit y push de la rama deploy/render.")


if __name__ == "__main__":
    main()
