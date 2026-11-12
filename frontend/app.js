"use strict";

const find = (id) => document.getElementById(id);
const source = find("source");
const paint = find("paint");
const highlight = find("highlight");
const gutter = find("gutter");
const examples = find("examples");
const optimize = find("optimize");
const security = find("security");
const compile = find("compile");
const run = find("run");
const tabs = [...document.querySelectorAll('[role="tab"]')];
const panels = new Map(tabs.map((tab) => [tab.id.slice(4), find(tab.getAttribute("aria-controls"))]));
const headers = "#include <iostream>\nusing namespace std;\n\n";
const demo = headers + [
  "int doble(int n) {",
  "    return n * 2;",
  "}",
  "",
  "int main() {",
  "    int base = 2 + 3 * 4;",
  "    int total = 0;",
  "    for (int i = 0; i < 3; ++i) {",
  "        total = total + doble(base + i);",
  "    }",
  '    cout << "Total: " << total << endl;',
  "    return 0;",
  "}",
  "",
].join("\n");
const demonstrations = new Map([
  ["Optimización y bucle", demo],
  ["Aritmética de punteros", headers + [
    "int main() {",
    "    int datos[3] = {10, 20, 30};",
    "    int* inicio = &datos[0];",
    "    int* ultimo = inicio + 2;",
    '    cout << *(inicio + 1) << " " << (ultimo - inicio) << endl;',
    "    return 0;",
    "}",
    "",
  ].join("\n")],
  ["Sentencias del AST", headers + [
    "int main() {",
    "    int n = 0;",
    "    do n = n + 1; while (n < 2);",
    "    while (n < 4) n = n + 1;",
    "    cout << n << endl;",
    "    return 0;",
    "}",
    "",
  ].join("\n")],
  ["Árbol de expresión", headers + [
    "int main() {",
    "    int a = 12, b = 2, c = 3, d = 4;",
    "    cout << ((a - b * c) + d) << endl;",
    "    return 0;",
    "}",
    "",
  ].join("\n")],
  ["Error léxico", "int main() {\n    return @;\n}\n"],
  ["Error sintáctico", "int main() {\n    int n = ;\n    return 0;\n}\n"],
  ["Error semántico", "int main() {\n    return desconocida;\n}\n"],
  ["Seguridad · índice constante", "int main() {\n    int datos[2] = {10, 20};\n    return datos[2];\n}\n"],
  ["Seguridad · índice dinámico", headers + "int main() {\n    int datos[2] = {10, 20};\n    int i = 2;\n    cout << datos[i] << endl;\n    return 0;\n}\n"],
  ["Seguridad · advertencia", headers + "int main() {\n    int x;\n    bool asignar = true;\n    if (asignar) x = 10;\n    cout << x << endl;\n    return 0;\n}\n"],
]);
const keywords = new Set("auto bool break case char class const continue default do double else false float for if int long namespace nullptr operator return struct switch template true typename using void while".split(" "));
const library = new Set(["cout", "endl", "string", "size", "length", "std", "max", "min"]);
const tokens = /\/\/.*|#[ \t]*include\b.*|"(?:\\.|[^"\\])*"?|'(?:\\.|[^'\\])*'?|\b\d+(?:\.\d+)?\b|\b[A-Za-z_]\w*\b/g;
let revision = 0;
let busy = false;
let diagnostics = [];

function element(tag, text = "", className = "") {
  const node = document.createElement(tag);
  if (text) node.textContent = text;
  if (className) node.className = className;
  return node;
}

function tokenClass(text) {
  if (text.startsWith("//")) return "comment";
  if (text.startsWith("#")) return "directive";
  if (text.startsWith('"') || text.startsWith("'")) return "string";
  if (/^\d/.test(text)) return "number";
  if (keywords.has(text)) return "keyword";
  if (library.has(text)) return "library";
  return "";
}

function syncEditor() {
  highlight.scrollTop = source.scrollTop;
  highlight.scrollLeft = source.scrollLeft;
  gutter.scrollTop = source.scrollTop;
}

function paintSource() {
  const marked = new Set(diagnostics.filter((d) => d.severity === "error").map((d) => d.line));
  const code = document.createDocumentFragment();
  const lines = document.createDocumentFragment();
  source.value.split("\n").forEach((text, i) => {
    const line = i + 1;
    const markedClass = marked.has(line) ? "marked" : "";
    const lineNumber = element("div", String(line), markedClass);
    const row = element("span", "", "line " + markedClass);
    let start = 0;
    for (const match of text.matchAll(tokens)) {
      row.append(text.slice(start, match.index));
      const kind = tokenClass(match[0]);
      row.append(kind ? element("span", match[0], "token-" + kind) : match[0]);
      start = match.index + match[0].length;
    }
    row.append(text.slice(start));
    code.append(row);
    lines.append(lineNumber);
  });
  paint.replaceChildren(code);
  gutter.replaceChildren(lines);
  syncEditor();
}

function showPosition() {
  const lines = source.value.slice(0, source.selectionStart).split("\n");
  find("position").textContent = "Línea " + lines.length + ", columna " + (lines.at(-1).length + 1);
}

function goTo(line, column) {
  const lines = source.value.split("\n");
  const first = lines.slice(0, line - 1).reduce((index, text) => index + text.length + 1, 0);
  const index = first + Math.min(column - 1, lines[line - 1].length);
  source.focus();
  source.setSelectionRange(index, Math.min(index + 1, source.value.length));
  const lineHeight = parseFloat(getComputedStyle(source).lineHeight);
  source.scrollTop = Math.max(0, (line - 3) * lineHeight);
  showPosition();
}

function selectTab(name, focus = false) {
  for (const tab of tabs) {
    const key = tab.id.slice(4);
    const selected = key === name;
    tab.setAttribute("aria-selected", String(selected));
    tab.tabIndex = selected ? 0 : -1;
    panels.get(key).hidden = !selected;
    if (selected && focus) tab.focus();
  }
  find("views").scrollTop = 0;
  if (name === "ast") find("ast-viewport")?.dispatchEvent(new Event("ast-visible"));
}

function empty(title, text) {
  const box = element("div", "", "empty");
  box.append(element("strong", title), document.createTextNode(text));
  return box;
}

function codeBlock(text, title) {
  const section = element("section");
  const code = element("pre", text, "code");
  code.tabIndex = 0;
  code.setAttribute("aria-label", title);
  section.append(element("h3", title), code);
  return section;
}

function renderTree(node, depth = 0) {
  const item = element("li");
  if (!node.children.length) {
    item.append(element("span", astNodeLabel(node), "leaf"));
    return item;
  }
  const details = element("details");
  details.append(element("summary", astNodeLabel(node)));
  const showChildren = () => {
    const list = element("ul");
    for (const child of node.children) list.append(renderTree(child, depth + 1));
    details.append(list);
  };
  if (depth < 4) {
    details.open = true;
    showChildren();
  } else {
    details.addEventListener("toggle", () => {
      if (details.open && details.childElementCount === 1) showChildren();
    });
  }
  item.append(details);
  return item;
}

function renderDiagnostics(result) {
  const view = panels.get("diagnostics");
  const badge = find("count");
  badge.hidden = !diagnostics.length;
  badge.textContent = diagnostics.length;
  if (!diagnostics.length) {
    view.replaceChildren(result
      ? empty("Sin diagnósticos", security.checked
        ? "El análisis no detectó errores ni advertencias de seguridad."
        : "El análisis de seguridad está desactivado.")
      : empty("Aún no hay resultados", "Compila el código para ver los diagnósticos y explorar las fases."));
    return;
  }
  view.replaceChildren();
  for (const d of diagnostics) {
    const article = element("article", "", "diagnostic " + d.severity);
    const header = element("header");
    header.append(element("strong", (d.severity === "warning" ? "Advertencia" : "Error") + " · " + d.phase));
    if (d.line !== null) {
      const location = element("button", "L" + d.line + ":" + d.column);
      location.type = "button";
      location.setAttribute("aria-label", "Ir a la línea " + d.line + ", columna " + d.column);
      location.addEventListener("click", () => goTo(d.line, d.column));
      header.append(location);
    }
    article.append(header, element("p", d.message));
    view.append(article);
  }
}

function render(result) {
  diagnostics = result?.diagnostics ?? [];
  paintSource();
  renderDiagnostics(result);

  const ast = panels.get("ast");
  if (result?.ast) {
    ast.replaceChildren(...renderAST(result.ast));
  } else {
    ast.replaceChildren(empty("AST no disponible", "Se muestra cuando el análisis sintáctico termina correctamente."));
  }

  const ir = panels.get("ir");
  if (result?.irBefore) {
    const comparison = element("div", "", "compare");
    comparison.append(codeBlock(result.irBefore, "ANTES DE OPTIMIZAR"));
    if (result.irAfter) comparison.append(codeBlock(result.irAfter, "DESPUÉS DE OPTIMIZAR"));
    let description = "IR del análisis de flujo de control. Cada bloque muestra una operación y sus posibles sucesores.";
    if (!result.irAfter) description += " La compilación se detuvo antes de completar la segunda vista.";
    else if (!optimize.checked) description += " Las optimizaciones están desactivadas.";
    ir.replaceChildren(element("p", description, "view-note"), comparison);
  } else {
    ir.replaceChildren(empty("IR no disponible", "Se genera después de validar los tipos del programa."));
  }

  const assembly = panels.get("asm");
  if (result?.assembly) {
    const actions = element("div", "", "view-actions");
    const download = element("button", "Descargar programa.s");
    download.type = "button";
    download.addEventListener("click", () => {
      const url = URL.createObjectURL(new Blob([result.assembly], { type: "text/plain" }));
      const link = element("a");
      link.href = url;
      link.download = "programa.s";
      link.click();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
    });
    actions.append(download);
    assembly.replaceChildren(actions, codeBlock(result.assembly, "ENSAMBLADOR x86-64"));
  } else {
    assembly.replaceChildren(empty("Ensamblador no disponible", "Compila un programa válido para generar el archivo .s."));
  }

  const output = panels.get("output");
  const execution = result?.execution;
  if (execution) {
    const grid = element("div", "", "output-grid");
    const status = execution.timedOut ? "Tiempo agotado"
      : execution.signal ? "Señal " + execution.signal : "Código de salida " + execution.exitCode;
    grid.append(element("p", status + " · " + execution.elapsedMs + " ms", "view-note"),
      codeBlock(execution.stdout || "(sin salida)", "SALIDA ESTÁNDAR"),
      codeBlock(execution.stderr || "(sin mensajes)", "SALIDA DE ERROR"));
    output.replaceChildren(grid);
  } else {
    output.replaceChildren(empty("El programa aún no se ha ejecutado", "Usa Ejecutar para compilar, ensamblar y ver el resultado."));
  }
}

function setStatus(text, state) {
  const status = find("status");
  status.textContent = text;
  status.dataset.state = state;
}

function changed() {
  revision++;
  render(null);
  setStatus("Pendiente", "ready");
  find("timing").textContent = "Cambios sin compilar";
}

async function submit(execute) {
  if (busy) return;
  busy = true;
  compile.disabled = run.disabled = true;
  const submitted = revision;
  setStatus(execute ? "Compilando y ejecutando…" : "Compilando…", "ready");
  try {
    const response = await fetch("/api/compile", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ source: source.value, optimize: optimize.checked, security: security.checked, execute }),
    });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || "La solicitud falló.");
    if (submitted !== revision) return;
    render(result);
    find("timing").textContent = "Compilación · " + result.compileMs + " ms";
    const errors = result.diagnostics.some((d) => d.severity === "error");
    if (errors || !result.ok) {
      setStatus("Con errores", "error");
      selectTab("diagnostics");
    } else if (result.diagnostics.length) {
      setStatus("Con advertencias", "warning");
      selectTab("diagnostics");
    } else {
      setStatus(execute ? "Ejecución finalizada" : "Compilación correcta", "ok");
      if (execute) selectTab("output");
    }
  } catch (error) {
    if (submitted !== revision) return;
    render({ diagnostics: [{ phase: "interfaz", message: error.message, severity: "error", line: null }] });
    setStatus("Error de conexión", "error");
    selectTab("diagnostics");
  } finally {
    busy = false;
    compile.disabled = run.disabled = false;
  }
}

function addExamples(label, entries) {
  const group = element("optgroup");
  group.label = label;
  for (const [name, code] of entries) {
    demonstrations.set(name, code);
    const option = element("option", name);
    option.value = name;
    group.append(option);
  }
  examples.append(group);
}

addExamples("Demostraciones", demonstrations);
fetch("/api/examples").then((response) => response.json()).then((items) => {
  addExamples("Archivos del proyecto", items.map(({ name, source: code }) => [name, code]));
}).catch((error) => console.warn("No se pudieron cargar los ejemplos del proyecto.", error));

examples.addEventListener("change", () => {
  if (!demonstrations.has(examples.value)) return;
  source.value = demonstrations.get(examples.value);
  source.setSelectionRange(0, 0);
  source.scrollTop = source.scrollLeft = 0;
  changed();
  showPosition();
});
source.addEventListener("input", () => {
  examples.value = "";
  changed();
  showPosition();
});
source.addEventListener("scroll", syncEditor);
source.addEventListener("selectionchange", showPosition);
source.addEventListener("keyup", showPosition);
source.addEventListener("click", showPosition);
optimize.addEventListener("change", changed);
security.addEventListener("change", changed);
compile.addEventListener("click", () => submit(false));
run.addEventListener("click", () => submit(true));
document.addEventListener("keydown", (event) => {
  if (event.key !== "Enter" || !(event.ctrlKey || event.metaKey)) return;
  event.preventDefault();
  submit(event.shiftKey);
});
for (const tab of tabs) tab.addEventListener("click", () => selectTab(tab.id.slice(4)));
find("tabs").addEventListener("keydown", (event) => {
  const active = tabs.indexOf(event.target);
  if (active < 0) return;
  const next = {
    ArrowLeft: (active + tabs.length - 1) % tabs.length,
    ArrowRight: (active + 1) % tabs.length,
    Home: 0,
    End: tabs.length - 1,
  }[event.key];
  if (next === undefined) return;
  event.preventDefault();
  selectTab(tabs[next].id.slice(4), true);
});

source.value = demo;
source.setSelectionRange(0, 0);
examples.value = "Optimización y bucle";
render(null);

