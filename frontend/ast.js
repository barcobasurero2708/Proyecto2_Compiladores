"use strict";

const binaryOperators = new Set(["+", "-", "*", "/", "%", "**", "<", "<=", ">", ">=", "==", "!=", "&&", "||"]);
const astOperators = new Set([...binaryOperators, "neg", "!", "?:"]);
const nodeHeight = 48;
const levelGap = 86;
const diagramPadding = 22;

function astOperation(node) {
  return node.label.split("  ", 1)[0];
}

function astNodeLabel(node) {
  return node.className ? node.className + " · " + node.label : node.label;
}

function astBranches(root) {
  const branches = [{ label: "Programa completo", node: root }];
  let expressions = 0;
  function visit(node, parentIsOperator = false) {
    const operation = astOperation(node);
    const isOperator = astOperators.has(operation);
    if (operation === "función") {
      branches.push({ label: node.label.replace("  ", " · "), node });
    } else if (isOperator && !parentIsOperator) {
      branches.push({ label: "Expresión " + (++expressions) + " · " + operation, node });
    }
    for (const child of node.children) visit(child, isOperator);
  }
  visit(root);
  return branches;
}

function astSvg(tag, attributes, text = "") {
  const node = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const [name, value] of Object.entries(attributes)) node.setAttribute(name, value);
  if (text) node.textContent = text;
  return node;
}

function layoutAST(root, expanded) {
  const nodes = [];
  let height = 0;

  function measure(node, depth) {
    const operation = astOperation(node);
    const detail = node.label.slice(operation.length + 2);
    const shortLabel = astOperators.has(operation) ? operation
      : operation === "variable" ? detail : node.label.replace("  ", " ");
    const characters = Array.from(shortLabel);
    const label = characters.length > 24 ? characters.slice(0, 23).join("") + "…" : shortLabel;
    const width = Math.max(76, Array.from(label).length * 8 + 22, (node.className?.length ?? 0) * 8 + 22);
    const open = node.children.length > 0 && (expanded.get(node)
      ?? (depth === 0 || (depth < 3 && node.children.length <= 8)));
    const children = open ? node.children.map((child) => measure(child, depth + 1)) : [];
    const childWidth = children.reduce((sum, child) => sum + child.span + 22, -22);
    height = Math.max(height, depth * levelGap + nodeHeight);
    return { node, operation, label, width, open, children, childWidth,
      span: Math.max(width, childWidth), y: diagramPadding + depth * levelGap };
  }

  function place(box, left) {
    box.x = left + box.span / 2;
    nodes.push(box);
    let start = box.x - box.childWidth / 2;
    for (const child of box.children) {
      place(child, start);
      start += child.span + 22;
    }
  }

  const layout = measure(root, 0);
  place(layout, diagramPadding);
  return { nodes, width: layout.span + diagramPadding * 2, height: height + diagramPadding * 2 };
}

function astDiagram(root) {
  const container = element("div");
  const note = element("p", "Cada nodo muestra su clase del compilador y el detalle del código. Pulsa un nodo para abrir o cerrar su rama.", "view-note");
  const toolbar = element("div", "", "ast-toolbar");
  const branchLabel = element("label", "Rama");
  const branchSelect = element("select");
  const branches = astBranches(root);
  branches.forEach(({ label }, index) => {
    const option = element("option", label);
    option.value = index;
    branchSelect.append(option);
  });
  branchLabel.append(branchSelect);
  const zoomControls = element("div", "", "ast-zoom");
  const zoomOut = element("button", "−");
  const zoomIn = element("button", "+");
  const reset = element("button", "100 %");
  zoomOut.setAttribute("aria-label", "Alejar diagrama");
  zoomIn.setAttribute("aria-label", "Acercar diagrama");
  reset.setAttribute("aria-label", "Restablecer tamaño y centrar el diagrama");
  zoomControls.append(zoomOut, reset, zoomIn);
  toolbar.append(branchLabel, zoomControls);
  const viewport = element("div", "", "ast-viewport");
  viewport.id = "ast-viewport";
  viewport.tabIndex = 0;
  viewport.setAttribute("aria-label", "Diagrama del árbol sintáctico. Desplázate para ver más nodos.");
  container.append(note, toolbar, viewport);

  let zoom = 1;
  const expanded = new Map();
  let layout;
  let svg;
  let needsCenter = true;

  function center() {
    if (!viewport.clientWidth) {
      needsCenter = true;
      return;
    }
    viewport.scrollLeft = (viewport.scrollWidth - viewport.clientWidth) / 2;
    viewport.scrollTop = 0;
    needsCenter = false;
  }

  function resize() {
    svg.setAttribute("width", Math.ceil(layout.width * zoom));
    svg.setAttribute("height", Math.ceil(layout.height * zoom));
    reset.textContent = Math.round(zoom * 100) + " %";
    zoomOut.disabled = zoom <= 0.5;
    zoomIn.disabled = zoom >= 1.8;
  }

  function draw() {
    layout = layoutAST(branches[Number(branchSelect.value)].node, expanded);
    svg = astSvg("svg", { viewBox: "0 0 " + layout.width + " " + layout.height, class: "ast-canvas" });
    const edges = astSvg("g", { "aria-hidden": "true" });
    const boxes = astSvg("g", {});
    svg.append(edges, boxes);
    for (const box of layout.nodes) {
      box.children.forEach((child, index) => {
        const startY = box.y + nodeHeight;
        edges.append(astSvg("line", { x1: box.x, y1: startY, x2: child.x, y2: child.y, class: "ast-edge" }));
        if (box.children.length === 2 && binaryOperators.has(box.operation)) {
          const x = (box.x + child.x) / 2 + (index ? 11 : -11);
          edges.append(astSvg("text", { x, y: (startY + child.y) / 2, class: "ast-edge-label" }, index ? "der." : "izq."));
        }
      });
      const fullLabel = astNodeLabel(box.node);
      const group = astSvg("g", { class: "ast-node", transform: "translate(" + box.x + " " + box.y + ")",
        "aria-label": fullLabel });
      group.append(astSvg("title", {}, fullLabel), astSvg("rect", {
        x: -box.width / 2, y: 0, width: box.width, height: nodeHeight,
        rx: 10, class: box.node.className ? "ast-shape typed" : "ast-shape",
      }));
      if (box.node.className) {
        group.append(astSvg("text", { x: 0, y: 16, class: "ast-class" }, box.node.className));
      }
      group.append(astSvg("text", {
        x: 0, y: box.node.className ? 33 : nodeHeight / 2, class: "ast-label",
      }, box.label));
      if (box.node.children.length) {
        group.setAttribute("role", "button");
        group.setAttribute("tabindex", "0");
        group.setAttribute("aria-expanded", String(box.open));
        group.append(astSvg("circle", { cx: 0, cy: nodeHeight, r: 7, class: "ast-toggle" }),
          astSvg("text", { x: 0, y: nodeHeight, class: "ast-toggle-label", "aria-hidden": "true" }, box.open ? "−" : "+"));
        function toggle() {
          const { scrollLeft, scrollTop } = viewport;
          expanded.set(box.node, !box.open);
          draw();
          viewport.scrollLeft = scrollLeft;
          viewport.scrollTop = scrollTop;
          const index = layout.nodes.findIndex((next) => next.node === box.node);
          svg.querySelectorAll(".ast-node")[index].focus();
        }
        group.addEventListener("click", toggle);
        group.addEventListener("keydown", (event) => {
          if (event.key !== "Enter" && event.key !== " ") return;
          event.preventDefault();
          toggle();
        });
      } else {
        group.setAttribute("role", "img");
      }
      boxes.append(group);
    }
    resize();
    viewport.replaceChildren(svg);
  }

  function adjustZoom(step) {
    const x = (viewport.scrollLeft + viewport.clientWidth / 2) / zoom;
    const y = (viewport.scrollTop + viewport.clientHeight / 2) / zoom;
    zoom = Math.max(0.5, Math.min(1.8, Math.round((zoom + step) * 100) / 100));
    resize();
    viewport.scrollLeft = x * zoom - viewport.clientWidth / 2;
    viewport.scrollTop = y * zoom - viewport.clientHeight / 2;
  }

  zoomOut.addEventListener("click", () => adjustZoom(-0.15));
  zoomIn.addEventListener("click", () => adjustZoom(0.15));
  reset.addEventListener("click", () => {
    zoom = 1;
    resize();
    center();
  });
  branchSelect.addEventListener("change", () => {
    expanded.clear();
    draw();
    center();
  });
  viewport.addEventListener("ast-visible", () => {
    if (needsCenter) center();
  });
  draw();
  requestAnimationFrame(center);
  return container;
}

function renderAST(root) {
  const modes = element("div", "", "ast-mode");
  const diagramButton = element("button", "Diagrama");
  const listButton = element("button", "Lista");
  const diagram = astDiagram(root);
  const tree = element("ul", "", "tree");
  tree.append(renderTree(root));
  function showDiagram(shown) {
    diagramButton.setAttribute("aria-pressed", String(shown));
    listButton.setAttribute("aria-pressed", String(!shown));
    diagram.hidden = !shown;
    tree.hidden = shown;
    if (shown) diagram.querySelector(".ast-viewport").dispatchEvent(new Event("ast-visible"));
  }
  diagramButton.addEventListener("click", () => showDiagram(true));
  listButton.addEventListener("click", () => showDiagram(false));
  modes.append(diagramButton, listButton);
  showDiagram(true);
  return [modes, diagram, tree];
}
