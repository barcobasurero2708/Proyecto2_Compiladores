#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include "ast.h"
#include "visitor.h"

using namespace std;

// Registros donde van los argumentos de una llamada (convención System V)
static const char* REG_ARGS[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

// Tipo (sin valor concreto) que corresponde a un nombre de tipo
static Type tipoDe(const string& nombre) {
    return nombre == "bool" ? Type(false) : Type(0);
}

// =====================================================
// ====================== accept =======================
// =====================================================

Type BinaryExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type NumberExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type IdExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type IfExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type FCallExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type MaxMinExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type CountExp::accept(Visitor* visitor)   { return visitor->visit(this); }

void AssignStmt::accept(Visitor* visitor)  { visitor->visit(this); }
void PrintStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void IfStmt::accept(Visitor* visitor)      { visitor->visit(this); }
void WhileStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void DoWhileStmt::accept(Visitor* visitor) { visitor->visit(this); }
void BreakStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void ReturnStmt::accept(Visitor* visitor)  { visitor->visit(this); }

void VarDec::accept(Visitor* visitor)   { visitor->visit(this); }
void Body::accept(Visitor* visitor)     { visitor->visit(this); }
void FunDec::accept(Visitor* visitor)   { visitor->visit(this); }
void Programa::accept(Visitor* visitor) { visitor->visit(this); }

// =====================================================
// ================== GencodeVisitor ===================
// =====================================================

// Crea el generador de código, que escribe el ensamblador en out.
// Necesita el TypeChecker ya ejecutado para saber cuánta pila reservar en cada función.
GencodeVisitor::GencodeVisitor(ostream& out, TypeChecker* checker) : out(out), checker(checker) {}

// Genera el código de todo el programa
void GencodeVisitor::gencode(Programa* program) {
    program->accept(this);
}

// Genera el código de left op right y deja el resultado en %rax
Type GencodeVisitor::visit(BinaryExp* exp) {
    Type l = exp->left->accept(this);
    out << "pushq %rax" << endl;
    pila++;
    Type r = exp->right->accept(this);
    out << "movq %rax, %rcx" << endl;
    out << "popq %rax" << endl;
    pila--;
    switch (exp->op) {
        case PLUS_OP:
            out << "addq %rcx, %rax" << endl;
            break;
        case MINUS_OP:
            out << "subq %rcx, %rax" << endl;
            break;
        case MUL_OP:
            out << "imulq %rcx, %rax" << endl;
            break;
        case DIV_OP:
            out << "cqto" << endl;                 // extiende el signo de %rax a %rdx
            out << "idivq %rcx" << endl;           // cociente en %rax
            break;
        case POW_OP: {
            // %rax = base ** exponente, multiplicando %rcx veces
            int id = etiquetas++;
            out << "movq %rax, %rdx" << endl;
            out << "movq $1, %rax" << endl;
            out << "pow_loop_" << id << ":" << endl;
            out << "cmpq $0, %rcx" << endl;
            out << "jle pow_end_" << id << endl;
            out << "imulq %rdx, %rax" << endl;
            out << "decq %rcx" << endl;
            out << "jmp pow_loop_" << id << endl;
            out << "pow_end_" << id << ":" << endl;
            break;
        }
        case LE_OP:
            out << "cmpq %rcx, %rax" << endl;
            out << "setle %al" << endl;
            out << "movzbq %al, %rax" << endl;
            break;
        default:
            break;
    }
    // Valor de cada operando visto como entero (un bool vale 1 o 0)
    int a = l.isBool() ? l.boolValue : l.intValue;
    int b = r.isBool() ? r.boolValue : r.intValue;
    switch (exp->op) {
        case PLUS_OP:  return Type(a + b);
        case MINUS_OP: return Type(a - b);
        case MUL_OP:   return Type(a * b);
        case DIV_OP:   return Type(b != 0 ? a / b : 0);
        case POW_OP: {
            int p = 1;
            for (int i = 0; i < b; i++) p *= a;
            return Type(p);
        }
        case LE_OP:    return Type(a <= b);
        default:       return Type();
    }
}

// Carga el literal en %rax
Type GencodeVisitor::visit(NumberExp* exp) {
    out << "movq $" << exp->value << ", %rax" << endl;
    if (exp->esBool) return Type(exp->value != 0);   // true / false
    return Type(exp->value);
}

// Carga el valor de la variable desde la pila en %rax
Type GencodeVisitor::visit(IdExp* exp) {
    out << "movq " << posicion.lookup(exp->value) * -TAM_VARIABLE << "(%rbp), %rax" << endl;
    return tipos.lookup(exp->value);
}

// Genera el código del ifexp: deja en %rax el valor de la rama elegida
Type GencodeVisitor::visit(IfExp* exp) {
    int id = etiquetas++;
    Type c = exp->condicion->accept(this);
    out << "cmpq $0, %rax" << endl;
    out << "je ifexp_else_" << id << endl;
    Type t = exp->thenExp->accept(this);
    out << "jmp ifexp_end_" << id << endl;
    out << "ifexp_else_" << id << ":" << endl;
    Type e = exp->elseExp->accept(this);
    out << "ifexp_end_" << id << ":" << endl;
    bool cond = c.isBool() ? c.boolValue : c.intValue != 0;
    return cond ? t : e;
}

Type GencodeVisitor::visit(MaxMinExp* exp) {
    exp->argumentos[0]->accept(this);
    for (size_t i = 1; i < exp->argumentos.size(); i++) {
        out << "pushq %rax" << endl;
        pila++;
        exp->argumentos[i]->accept(this);
        out << "movq %rax, %rcx" << endl;
        out << "popq %rax" << endl;
        pila--;
        out << "cmpq %rcx, %rax" << endl;
        out << (exp->esMax ? "cmovl %rcx, %rax" : "cmovg %rcx, %rax") << endl;
    }
    return Type(0);
}

Type GencodeVisitor::visit(CountExp* exp) {
    out << "pushq $0" << endl;                  // acumulador en el tope de la pila
    pila++;
    for (Exp* arg : exp->argumentos) {
        arg->accept(this);
        out << "addq %rax, (%rsp)" << endl;     // cada bool vale 0 o 1
    }
    out << "popq %rax" << endl;
    pila--;
    return Type(0);
}

// Llamada: evalúa los argumentos, los pasa en registros y llama a la función.
// El resultado queda en %rax.
Type GencodeVisitor::visit(FCallExp* exp) {
    for (Exp* arg : exp->argumentos) {
        arg->accept(this);
        out << "pushq %rax" << endl;
        pila++;
    }
    for (int i = (int)exp->argumentos.size() - 1; i >= 0; i--) {
        out << "popq " << REG_ARGS[i] << endl;
        pila--;
    }
    // La pila debe estar alineada a 16 bytes en el call
    bool alinear = pila % 2 != 0;
    if (alinear) out << "subq $8, %rsp" << endl;
    out << "call " << exp->nombre << endl;
    if (alinear) out << "addq $8, %rsp" << endl;
    return tipoDe(funciones[exp->nombre]->tipo);
}

// Evalúa la expresión y la guarda en la posición de la variable
void GencodeVisitor::visit(AssignStmt* stm) {
    Type t = stm->e->accept(this);
    tipos.update(stm->id, t);
    out << "movq %rax, " << posicion.lookup(stm->id) * -TAM_VARIABLE << "(%rbp)" << endl;
}

// Evalúa la expresión y la imprime con printf
void GencodeVisitor::visit(PrintStmt* stm) {
    stm->e->accept(this);
    out << "movq %rax, %rsi" << endl;
    out << "leaq print_fmt(%rip), %rdi" << endl;
    out << "movl $0, %eax" << endl;
    out << "call printf@PLT" << endl;
}

// Genera el código del if: si la condición es 0 salta a la rama else
void GencodeVisitor::visit(IfStmt* stm) {
    int id = etiquetas++;
    stm->condicion->accept(this);
    out << "cmpq $0, %rax" << endl;
    out << "je else_" << id << endl;
    for (Stmt* s : stm->thenList) s->accept(this);
    out << "jmp endif_" << id << endl;
    out << "else_" << id << ":" << endl;
    for (Stmt* s : stm->elseList) s->accept(this);
    out << "endif_" << id << ":" << endl;
}

// Genera el código del while: evalúa la condición al inicio de cada vuelta
void GencodeVisitor::visit(WhileStmt* stm) {
    int id = etiquetas++;
    string fin = "endwhile_" + to_string(id);
    out << "while_" << id << ":" << endl;
    stm->condicion->accept(this);
    out << "cmpq $0, %rax" << endl;
    out << "je " << fin << endl;
    finDeBucle.push_back(fin);
    for (Stmt* s : stm->body) s->accept(this);
    finDeBucle.pop_back();
    out << "jmp while_" << id << endl;
    out << fin << ":" << endl;
}

// Genera el código del do-while: ejecuta el cuerpo y repite mientras la condición no sea 0
void GencodeVisitor::visit(DoWhileStmt* stm) {
    int id = etiquetas++;
    string fin = "enddowhile_" + to_string(id);
    out << "dowhile_" << id << ":" << endl;
    finDeBucle.push_back(fin);
    for (Stmt* s : stm->body) s->accept(this);
    finDeBucle.pop_back();
    stm->condicion->accept(this);
    out << "cmpq $0, %rax" << endl;
    out << "jne dowhile_" << id << endl;
    out << fin << ":" << endl;
}

// Genera el código del break: salta a la salida del bucle más interno
void GencodeVisitor::visit(BreakStmt* stm) {
    out << "jmp " << finDeBucle.back() << endl;
}

// Genera el código del return: deja el valor en %rax y salta al epílogo de la función
void GencodeVisitor::visit(ReturnStmt* stm) {
    stm->exp->accept(this);
    out << "jmp .Lfin_" << funcionActual << endl;
}

// Declara cada variable en el nivel actual: le asigna una posición en la pila y su tipo
void GencodeVisitor::visit(VarDec* dec) {
    for (const string& v : dec->variables) {
        posicion.add_var(v, contador++);
        tipos.add_var(v, tipoDe(dec->tipo));
        out << "movq $0, " << posicion.lookup(v) * -TAM_VARIABLE << "(%rbp)" << endl;   // inicia en 0 / false
    }
}

// Procesa primero las declaraciones y luego las sentencias
void GencodeVisitor::visit(Body* body) {
    for (VarDec* d : body->declaraciones) d->accept(this);
    for (Stmt* s : body->sentencias) s->accept(this);
}

// Genera una función: prólogo, copia de parámetros a la pila, cuerpo y epílogo
void GencodeVisitor::visit(FunDec* fun) {
    funcionActual = fun->nombre;
    contador = 1;
    posicion.add_level();
    tipos.add_level();

    // Espacio en la pila según lo que contó el TypeChecker
    int espacio = checker->espacioPila(fun->nombre);

    out << fun->nombre << ":" << endl;
    out << "pushq %rbp" << endl;
    out << "movq %rsp, %rbp" << endl;
    if (espacio > 0) out << "subq $" << espacio << ", %rsp" << endl;

    // Los parámetros llegan en registros: se guardan en la pila como variables locales
    for (size_t i = 0; i < fun->parametros_id.size(); i++) {
        const string& p = fun->parametros_id[i];
        posicion.add_var(p, contador++);
        tipos.add_var(p, tipoDe(fun->parametros_tipos[i]));
        out << "movq " << REG_ARGS[i] << ", " << posicion.lookup(p) * -TAM_VARIABLE << "(%rbp)" << endl;
    }

    fun->cuerpo->accept(this);

    out << "movq $0, %rax" << endl;            // si no hubo return, devuelve 0
    out << ".Lfin_" << fun->nombre << ":" << endl;
    if (fun->nombre == "main") {
        out << "movq $0, %rax" << endl;        // main siempre termina con código 0
    }
    out << "leave" << endl;
    out << "ret" << endl;

    posicion.remove_level();
    tipos.remove_level();
}

// Genera el programa completo: sección de datos y el código de cada función
void GencodeVisitor::visit(Programa* program) {
    for (FunDec* f : program->funciones) funciones[f->nombre] = f;

    out << ".data" << endl;
    out << "print_fmt:.string \"%ld\\n\" " << endl;
    out << ".text" << endl;
    out << ".globl main" << endl;
    for (FunDec* f : program->funciones) f->accept(this);
    out << ".section .note.GNU-stack,\"\",@progbits" << endl;
}

// =====================================================
// ==================== TypeChecker ====================
// =====================================================

// Lanza un error de tipos con el mensaje dado
static void error(const string& msg) {
    throw runtime_error(msg);
}

// Verifica todo el programa
void TypeChecker::check(Programa* program) {
    program->accept(this);
}

// Bytes que debe reservar la función en la pila: 8 por variable (int o bool),
// redondeado a múltiplo de 16 para mantener la pila alineada
int TypeChecker::espacioPila(const string& funcion) {
    int bytes = numVariables[funcion] * TAM_VARIABLE;
    return ((bytes + 15) / 16) * 16;
}

// Verifica que la condición de una sentencia sea bool
void TypeChecker::esperarBool(Exp* condicion, const string& donde) {
    Type c = condicion->accept(this);
    if (!c.isBool()) {
        error("la condición del " + donde + " debe ser bool, pero es " + c.kindName());
    }
}

// ---------- Expresiones ----------

// Operadores aritméticos: int op int -> int.  Comparación: int <= int -> bool
Type TypeChecker::visit(BinaryExp* exp) {
    Type l = exp->left->accept(this);
    Type r = exp->right->accept(this);
    string op = Exp::binopToChar(exp->op);
    if (!l.isInt() || !r.isInt()) {
        error("el operador '" + op + "' necesita operandos int, pero recibió "
              + l.kindName() + " " + op + " " + r.kindName());
    }
    if (exp->op == LE_OP) return Type(false);   // bool
    return Type(0);                              // int
}

// Un número es int; true / false son bool
Type TypeChecker::visit(NumberExp* exp) {
    if (exp->esBool) return Type(false);
    return Type(0);
}

// Una variable tiene el tipo con el que fue declarada
Type TypeChecker::visit(IdExp* exp) {
    if (!tipos.check(exp->value)) {
        error("variable no declarada: " + exp->value);
    }
    return tipos.lookup(exp->value);
}

// ifexp(c, e1, e2): c debe ser bool y ambas ramas del mismo tipo
Type TypeChecker::visit(IfExp* exp) {
    Type c = exp->condicion->accept(this);
    if (!c.isBool()) {
        error("la condición de ifexp debe ser bool, pero es " + c.kindName());
    }
    Type t = exp->thenExp->accept(this);
    Type e = exp->elseExp->accept(this);
    if (t.kind != e.kind) {
        error("las ramas de ifexp deben tener el mismo tipo, pero son "
              + t.kindName() + " y " + e.kindName());
    }
    return t;
}

Type TypeChecker::visit(MaxMinExp* exp) {
    string nombre = exp->esMax ? "max" : "min";
    for (size_t i = 0; i < exp->argumentos.size(); i++) {
        Type t = exp->argumentos[i]->accept(this);
        if (!t.isInt()) {
            error("el argumento " + to_string(i + 1) + " de " + nombre
                  + " debe ser int, pero es " + t.kindName());
        }
    }
    return Type(0);
}

// count: cero o más argumentos bool, devuelve int
Type TypeChecker::visit(CountExp* exp) {
    for (size_t i = 0; i < exp->argumentos.size(); i++) {
        Type t = exp->argumentos[i]->accept(this);
        if (!t.isBool()) {
            error("el argumento " + to_string(i + 1) + " de count debe ser bool, pero es "
                  + t.kindName());
        }
    }
    return Type(0);
}

// f(a1, ..., an): la función existe, recibe n argumentos y cada uno tiene el tipo de su parámetro
Type TypeChecker::visit(FCallExp* exp) {
    if (!funciones.count(exp->nombre)) {
        error("función no declarada: " + exp->nombre);
    }
    FunDec* f = funciones[exp->nombre];
    if (exp->argumentos.size() != f->parametros_id.size()) {
        error("la función " + f->nombre + " espera " + to_string(f->parametros_id.size())
              + " argumento(s), pero recibió " + to_string(exp->argumentos.size()));
    }
    for (size_t i = 0; i < exp->argumentos.size(); i++) {
        Type t = exp->argumentos[i]->accept(this);
        if (t.kindName() != f->parametros_tipos[i]) {
            error("el argumento " + to_string(i + 1) + " de " + f->nombre + " debe ser "
                  + f->parametros_tipos[i] + ", pero es " + t.kindName());
        }
    }
    return tipoDe(f->tipo);
}

// ---------- Sentencias ----------

// x = e: x declarada y e del mismo tipo
void TypeChecker::visit(AssignStmt* stm) {
    if (!tipos.check(stm->id)) {
        error("variable no declarada: " + stm->id);
    }
    Type declarado = tipos.lookup(stm->id);
    Type t = stm->e->accept(this);
    if (t.kind != declarado.kind) {
        error("no se puede asignar " + t.kindName() + " a la variable "
              + declarado.kindName() + " " + stm->id);
    }
}

// print(e): e debe tener un valor (int o bool)
void TypeChecker::visit(PrintStmt* stm) {
    Type t = stm->e->accept(this);
    if (t.isVoid()) {
        error("print necesita una expresión int o bool");
    }
}

// if: condición bool y se verifican ambas ramas
void TypeChecker::visit(IfStmt* stm) {
    esperarBool(stm->condicion, "if");
    for (Stmt* s : stm->thenList) s->accept(this);
    for (Stmt* s : stm->elseList) s->accept(this);
}

// while: condición bool y se verifica el cuerpo
void TypeChecker::visit(WhileStmt* stm) {
    esperarBool(stm->condicion, "while");
    bucles++;
    for (Stmt* s : stm->body) s->accept(this);
    bucles--;
}

// do-while: se verifica el cuerpo y la condición debe ser bool
void TypeChecker::visit(DoWhileStmt* stm) {
    bucles++;
    for (Stmt* s : stm->body) s->accept(this);
    bucles--;
    esperarBool(stm->condicion, "do-while");
}

// break: solo dentro de un bucle
void TypeChecker::visit(BreakStmt* stm) {
    if (bucles == 0) {
        error("'break' fuera de un bucle");
    }
}

// return e: e debe tener el tipo de la función
void TypeChecker::visit(ReturnStmt* stm) {
    Type t = stm->exp->accept(this);
    if (t.kindName() != funcionActual->tipo) {
        error("la función " + funcionActual->nombre + " debe devolver "
              + funcionActual->tipo + ", pero return devuelve " + t.kindName());
    }
}

// ---------- Declaraciones y programa ----------

// var tipo ids: tipo válido y sin variables repetidas
void TypeChecker::visit(VarDec* dec) {
    if (dec->tipo != "int" && dec->tipo != "bool") {
        error("tipo desconocido: " + dec->tipo);
    }
    for (const string& v : dec->variables) {
        if (tipos.check(v)) {
            error("variable declarada dos veces: " + v);
        }
        tipos.add_var(v, tipoDe(dec->tipo));
        numVariables[funcionActual->nombre]++;      // una variable local más
    }
}

// Primero las declaraciones, luego las sentencias
void TypeChecker::visit(Body* body) {
    for (VarDec* d : body->declaraciones) d->accept(this);
    for (Stmt* s : body->sentencias) s->accept(this);
}

// Una función abre su propio nivel con sus parámetros y variables locales
void TypeChecker::visit(FunDec* fun) {
    funcionActual = fun;
    numFunciones++;
    numVariables[fun->nombre] = fun->parametros_id.size();   // los parámetros también ocupan pila
    tipos.add_level();
    for (size_t i = 0; i < fun->parametros_id.size(); i++) {
        const string& p = fun->parametros_id[i];
        if (tipos.check(p)) {
            error("parámetro repetido en " + fun->nombre + ": " + p);
        }
        tipos.add_var(p, tipoDe(fun->parametros_tipos[i]));
    }
    fun->cuerpo->accept(this);
    tipos.remove_level();
    funcionActual = nullptr;
}

// Registra todas las funciones (para permitir recursión y llamadas a funciones
// definidas más abajo) y luego verifica cada una
void TypeChecker::visit(Programa* program) {
    for (FunDec* f : program->funciones) {
        if (funciones.count(f->nombre)) {
            error("función declarada dos veces: " + f->nombre);
        }
        if (f->parametros_id.size() > 6) {
            error("la función " + f->nombre + " tiene más de 6 parámetros");
        }
        funciones[f->nombre] = f;
    }
    if (!funciones.count("main")) {
        error("falta la función main");
    }
    FunDec* m = funciones["main"];
    if (m->tipo != "int" || !m->parametros_id.empty()) {
        error("main debe declararse como: int main()");
    }
    for (FunDec* f : program->funciones) f->accept(this);
}
