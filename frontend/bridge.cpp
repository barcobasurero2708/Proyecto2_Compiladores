#include <algorithm>
#include <map>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include "diagnostico.h"
#include "gencode_visitor.h"
#include "parser.h"
#include "peephole.h"
#include "seguridad.h"
#include "type_checker_visitor.h"

namespace {

void jsonString(ostream& out, const string& text) {
    out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) {
            const char* hex = "0123456789abcdef";
            out << "\\u00" << hex[c >> 4] << hex[c & 15];
        } else {
            out << c;
        }
    }
    out << '"';
}

struct NodoVista {
    string operacion;
    string detalle;
    vector<NodoVista> hijos;
    string clase;
};

void escribirArbol(ostream& out, const NodoVista& nodo) {
    out << "{\"label\":";
    jsonString(out, nodo.operacion + (nodo.detalle.empty() ? "" : "  " + nodo.detalle));
    if (!nodo.clase.empty()) {
        out << ",\"className\":";
        jsonString(out, nodo.clase);
    }
    out << ",\"children\":[";
    for (size_t i = 0; i < nodo.hijos.size(); ++i) {
        if (i) out << ',';
        escribirArbol(out, nodo.hijos[i]);
    }
    out << "]}";
}

void escribirOperacion(ostream& out, const NodoVista& nodo) {
    out << nodo.operacion;
    if (!nodo.detalle.empty()) {
        out << ' ';
        jsonString(out, nodo.detalle);
    }
    if (!nodo.hijos.empty()) {
        out << '(';
        for (size_t i = 0; i < nodo.hijos.size(); ++i) {
            if (i) out << ", ";
            escribirOperacion(out, nodo.hijos[i]);
        }
        out << ')';
    }
}

class VistaAST : public Visitor {
    NodoVista resultado;

    template <typename Lista>
    NodoVista grupo(const string& etiqueta, const Lista& lista) {
        NodoVista grupo{etiqueta, "", {}};
        for (auto* hijo : lista) grupo.hijos.push_back(leer(hijo));
        return grupo;
    }

    Type expresion(Exp* nodo, const char* clase, string op, string detalle = "", vector<NodoVista> hijos = {}) {
        resultado = {std::move(op), std::move(detalle), std::move(hijos), clase};
        return nodo->tipoExp;
    }

    static string operador(BinaryOp op) {
        if (op == SHL_OP) return "<<";
        if (op == DIV_POT2_OP) return "div.pot2";
        if (op == MOD_POT2_OP) return "mod.pot2";
        return Exp::binopToChar(op);
    }

public:
    template <typename Nodo>
    NodoVista leer(Nodo* nodo) {
        if (!nodo) return {"omitido", "", {}};
        nodo->accept(this);
        return std::move(resultado);
    }

    Type visit(BinaryExp* e) override {
        return expresion(e, "BinaryExp", operador(e->op), e->sobrecarga, {leer(e->left), leer(e->right)});
    }
    Type visit(UnaryExp* e) override {
        return expresion(e, "UnaryExp", e->op == NOT_OP ? "!" : "neg", "", {leer(e->e)});
    }
    Type visit(StructExp* e) override {
        return expresion(e, "StructExp", "agregado", e->nombre, grupo("", e->valores).hijos);
    }
    Type visit(CampoExp* e) override { return expresion(e, "CampoExp", "campo", e->campo, {leer(e->objeto)}); }
    Type visit(NumberExp* e) override {
        string valor = to_string(e->value);
        string tipo = Type::nombre(e->tipo);
        if (e->tipo == Type::VOID) {
            tipo = "nullptr";
            valor.clear();
        } else if (e->tipo == Type::STRING || e->tipo == Type::CHAR) {
            ostringstream literal;
            jsonString(literal, e->tipo == Type::STRING ? e->texto : string(1, static_cast<char>(e->value)));
            valor = literal.str();
        } else if (e->tipo == Type::BOOL) {
            valor = e->value ? "true" : "false";
        } else if (Type::esFlotante(e->tipo)) {
            ostringstream numero;
            numero << setprecision(17) << e->valorFloat;
            valor = numero.str();
        }
        return expresion(e, "NumberExp", tipo, valor);
    }
    Type visit(IdExp* e) override { return expresion(e, "IdExp", "variable", e->value); }
    Type visit(IfExp* e) override {
        return expresion(e, "IfExp", "?:", "", {leer(e->condicion), leer(e->thenExp), leer(e->elseExp)});
    }
    Type visit(FCallExp* e) override {
        return expresion(e, "FCallExp", "llamar", e->nombre, grupo("", e->argumentos).hijos);
    }
    Type visit(ArrayExp* e) override {
        return expresion(e, "ArrayExp", "indexar", e->nombre, grupo("", e->indices).hijos);
    }
    Type visit(IncDecExp* e) override {
        string op = e->prefijo ? "pre" : "post";
        op += e->incremento ? "++" : "--";
        return expresion(e, "IncDecExp", op, e->nombre, grupo("", e->indices).hijos);
    }
    Type visit(DireccionExp* e) override { return expresion(e, "DireccionExp", "dirección", "", {leer(e->destino)}); }
    Type visit(DerefExp* e) override { return expresion(e, "DerefExp", "desreferenciar", "", {leer(e->puntero)}); }
    Type visit(LongitudExp* e) override { return expresion(e, "LongitudExp", "longitud", "", {leer(e->cadena)}); }

    void visit(AssignStmt* s) override {
        vector<NodoVista> operandos = grupo("", s->indices).hijos;
        operandos.push_back(leer(s->e));
        resultado = {"asignar", s->id + (s->campo.empty() ? "" : "." + s->campo), operandos, "AssignStmt"};
    }
    void visit(AssignPunteroStmt* s) override {
        resultado = {"asignar por puntero", "", {leer(s->puntero), leer(s->e)}, "AssignPunteroStmt"};
    }
    void visit(PrintStmt* s) override {
        vector<NodoVista> partes;
        for (Exp* e : s->expresiones) partes.push_back(e ? leer(e) : NodoVista{"endl", "", {}});
        resultado = {"cout", "", partes, "PrintStmt"};
    }
    void visit(IfStmt* s) override {
        resultado = {"if", "", {leer(s->condicion), grupo("entonces", s->thenList), grupo("si no", s->elseList)}, "IfStmt"};
    }
    void visit(WhileStmt* s) override {
        resultado = {"while", "", {leer(s->condicion), grupo("cuerpo", s->body)}, "WhileStmt"};
    }
    void visit(DoWhileStmt* s) override {
        resultado = {"do while", "", {grupo("cuerpo", s->body), leer(s->condicion)}, "DoWhileStmt"};
    }
    void visit(ForStmt* s) override {
        resultado = {"for", "", {{"inicio", "", {leer(s->init)}},
                                  {"condición", "", {leer(s->condicion)}},
                                  {"actualización", "", {leer(s->actualizacion)}}, grupo("cuerpo", s->body)}, "ForStmt"};
    }
    void visit(SwitchStmt* s) override {
        NodoVista arbol{"switch", "", {leer(s->condicion)}, "SwitchStmt"};
        size_t etiqueta = 0;
        for (size_t i = 0; i <= s->sentencias.size(); ++i) {
            while (etiqueta < s->etiquetas.size() && s->etiquetas[etiqueta].posicion == i) {
                Exp* e = s->etiquetas[etiqueta++].expresion;
                arbol.hijos.push_back(e ? NodoVista{"case", "", {leer(e)}, "SwitchStmt::Etiqueta"}
                                        : NodoVista{"default", "", {}, "SwitchStmt::Etiqueta"});
            }
            if (i < s->sentencias.size()) arbol.hijos.push_back(leer(s->sentencias[i]));
        }
        resultado = std::move(arbol);
    }
    void visit(BreakStmt*) override { resultado = {"break", "", {}, "BreakStmt"}; }
    void visit(ContinueStmt*) override { resultado = {"continue", "", {}, "ContinueStmt"}; }
    void visit(ReturnStmt* s) override {
        resultado = {"return", "", s->exp ? vector<NodoVista>{leer(s->exp)} : vector<NodoVista>{}, "ReturnStmt"};
    }
    void visit(BlockStmt* s) override {
        resultado = grupo("bloque", s->sentencias);
        resultado.clase = "BlockStmt";
    }
    void visit(ExprStmt* s) override { resultado = {"evaluar", "", {leer(s->e)}, "ExprStmt"}; }

    NodoVista variable(VarDec* d, size_t i) {
        string firma = (d->esConst ? "const " : "") + d->tipo + " " + d->variables[i];
        for (int dim : d->dimensiones[i]) firma += "[" + to_string(dim) + "]";
        NodoVista nodo{"declarar", firma, {}};
        if (d->inicializadores[i]) nodo.hijos.push_back(leer(d->inicializadores[i]));
        if (!d->listas[i].empty()) nodo.hijos.push_back(grupo("lista", d->listas[i]));
        return nodo;
    }

    void visit(VarDec* d) override {
        NodoVista dec{"declaraciones", "", {}, "VarDec"};
        for (size_t i = 0; i < d->variables.size(); ++i) dec.hijos.push_back(variable(d, i));
        resultado = std::move(dec);
    }

    void visit(Body* b) override {
        resultado = grupo("cuerpo", b->sentencias);
        resultado.clase = "Body";
    }

    void visit(FunDec* f) override {
        NodoVista fun{f->cuerpo ? "función" : "prototipo", f->tipo + " " + f->nombre, {}, "FunDec"};
        for (size_t i = 0; i < f->parametros_tipos.size(); ++i) {
            string parametro = (f->parametros_const[i] ? "const " : "") + f->parametros_tipos[i];
            if (f->parametros_ref[i]) parametro += '&';
            if (!f->parametros_id[i].empty()) parametro += " " + f->parametros_id[i];
            fun.hijos.push_back({"parámetro", parametro, {}});
        }
        if (f->cuerpo) fun.hijos.push_back(leer(f->cuerpo));
        resultado = std::move(fun);
    }

    void visit(Programa* p) override {
        NodoVista programa{"programa", "", {}, "Programa"};
        map<string, vector<string>> estructuras(p->estructuras.begin(), p->estructuras.end());
        for (const auto& [nombre, campos] : estructuras) {
            NodoVista agregado{"struct", nombre, {}};
            for (const string& campo : campos) agregado.hijos.push_back({"campo", "int " + campo, {}});
            programa.hijos.push_back(std::move(agregado));
        }
        map<string, Plantilla> plantillas(p->plantillas.begin(), p->plantillas.end());
        for (const auto& [nombre, p] : plantillas) {
            NodoVista plantilla{"template", nombre, {}, "Plantilla"};
            for (const string& tipo : p.tparams) plantilla.hijos.push_back({"tipo", tipo, {}});
            programa.hijos.push_back(std::move(plantilla));
        }
        for (FunDec* f : p->funciones) programa.hijos.push_back(leer(f));
        resultado = std::move(programa);
    }
};

// Cada registro muestra una operación del CFG real del optimizador y sus sucesores posibles.
void escribirIR(ostream& out, Programa* programa, VistaAST& vista) {
    for (FunDec* fun : programa->funciones) {
        if (!fun->cuerpo) continue;
        out << "func " << fun->nombre << " -> " << fun->tipo << '\n';
        ConstructorCFG cfg(fun);
        for (size_t i = 0; i < cfg.nodos.size(); ++i) {
            const NodoCFG& n = cfg.nodos[i];
            out << "  B" << i << "  ";
            if (i == 0) {
                out << "entrada";
            } else if (auto* d = dynamic_cast<VarDec*>(n.sentencia)) {
                auto it = find(d->variables.begin(), d->variables.end(), n.declarada);
                escribirOperacion(out, vista.variable(d, it - d->variables.begin()));
            } else if (dynamic_cast<IfStmt*>(n.sentencia) || dynamic_cast<ForStmt*>(n.sentencia)
                       || dynamic_cast<WhileStmt*>(n.sentencia) || dynamic_cast<SwitchStmt*>(n.sentencia)
                       || (!n.sentencia && n.expresion)) {
                escribirOperacion(out, {"bifurcar", "", {n.expresion ? vista.leer(n.expresion) : NodoVista{"bool", "true", {}}}});
            } else if (n.sentencia) {
                escribirOperacion(out, vista.leer(n.sentencia));
            } else {
                out << "unión";
            }
            if (!n.sucesores.empty()) {
                out << "    →";
                for (int sucesor : n.sucesores) out << " B" << sucesor;
            }
            out << '\n';
        }
        out << '\n';
    }
}

void diagnostico(const string& fase, const string& mensaje, int posicion = -1) {
    ofstream out("diagnostic.json");
    out << "{\"phase\":";
    jsonString(out, fase);
    out << ",\"message\":";
    jsonString(out, mensaje);
    out << ",\"offset\":" << posicion << '}';
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) return 2;
    bool optimizar = string(argv[1]) == "1";
    bool seguridad = string(argv[2]) == "1";
    ifstream archivo("source.cpp", ios::binary);
    if (!archivo) return 2;
    string fuente((istreambuf_iterator<char>(archivo)), istreambuf_iterator<char>());

    Scanner lexico(fuente.c_str());
    for (;;) {
        unique_ptr<Token> token(lexico.nextToken());
        if (token->type == Token::ERR) {
            diagnostico("léxico", "Token inválido: " + token->text);
            return 1;
        }
        if (token->type == Token::END) break;
    }

    Scanner scanner(fuente.c_str());
    Parser parser(&scanner);
    Programa* programa;
    try {
        programa = parser.parseProgram();
    } catch (const exception& e) {
        diagnostico("sintáctico", e.what());
        return 1;
    }

    VistaAST vista;
    {
        ofstream out("ast.json");
        escribirArbol(out, vista.leer(programa));
    }

    TypeChecker checker;
    try {
        checker.check(programa);
    } catch (const exception& e) {
        diagnostico("semántico", e.what());
        return 1;
    }

    string fase = "representación intermedia";
    try {
        {
            ofstream out("before.ir");
            escribirIR(out, programa, vista);
        }
        if (seguridad) {
            fase = "seguridad";
            analizarSeguridad(programa);
        }
        fase = "optimización";
        if (optimizar) optimizarAST(programa);
        {
            ofstream out("after.ir");
            escribirIR(out, programa, vista);
        }
        fase = "generación";
        ostringstream ensamblador;
        GencodeVisitor generador(ensamblador, &checker);
        generador.gencode(programa);
        ofstream out("program.s");
        out << (optimizar ? Mirilla().optimizar(ensamblador.str()) : ensamblador.str());
    } catch (const exception& e) {
        diagnostico(fase, e.what());
        return 1;
    }
}

