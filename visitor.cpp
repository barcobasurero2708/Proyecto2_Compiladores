#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "ast.h"
#include "visitor.h"

using namespace std;

// Registros donde van los argumentos de una llamada (convención System V)
static const char* REG_ARGS[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

// Tipo que corresponde a un nombre de tipo (de un arreglo, el de sus elementos: "int[][3]" -> int)
static Type::Kind kindDe(const string& tipo) {
    string nombre = tipo.substr(0, tipo.find('['));
    if (nombre == "bool")   return Type::BOOL;
    if (nombre == "int")    return Type::INT;
    if (nombre == "char")   return Type::CHAR;
    if (nombre == "float")  return Type::FLOAT;
    if (nombre == "double") return Type::DOUBLE;
    if (nombre == "long")   return Type::LONG;
    if (nombre == "string") return Type::STRING;
    return Type::VOID;
}

// Tipo completo de un nombre de tipo, incluidas las dimensiones: "int[][3]" -> int con dims {0, 3}
static Type tipoDe(const string& nombre) {
    Type t = Type::deKind(kindDe(nombre));
    size_t i = nombre.find('[');
    while (i != string::npos) {
        size_t fin = nombre.find(']', i);
        string n = nombre.substr(i + 1, fin - i - 1);
        t.dims.push_back(n.empty() ? 0 : stoi(n));
        i = nombre.find('[', fin);
    }
    return t;
}

// Bytes entre dos elementos consecutivos del índice k (cada elemento ocupa 8 bytes)
static long long pasoIndice(const vector<int>& dims, size_t k) {
    long long paso = TAM_VARIABLE;
    for (size_t j = k + 1; j < dims.size(); j++) paso *= dims[j];
    return paso;
}

// Cantidad de elementos de un arreglo
static long long totalElementos(const vector<int>& dims) {
    long long n = 1;
    for (int d : dims) n *= d;
    return n;
}

// Etiqueta en ensamblador de una función del programa. Se antepone "fn_" para que un
// nombre del programa no choque con funciones de la biblioteca C (printf, strcmp, ...)
static string etiquetaFuncion(const string& nombre) {
    return nombre == "main" ? "main" : "fn_" + nombre;
}

// Texto de un string listo para una directiva .string del ensamblador
static string escaparAsm(const string& texto) {
    string r;
    for (unsigned char ch : texto) {
        if (ch == '"' || ch == '\\') { r += '\\'; r += (char)ch; }
        else if (ch == '\n') r += "\\n";
        else if (ch == '\t') r += "\\t";
        else if (ch < 32 || ch >= 127) {
            char buf[8];
            snprintf(buf, sizeof buf, "\\%03o", ch);
            r += buf;
        }
        else r += (char)ch;
    }
    return r;
}

// =====================================================
// ====================== accept =======================
// =====================================================

Type BinaryExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type UnaryExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type ArrayExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type IncDecExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type NumberExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type IdExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type IfExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type FCallExp::accept(Visitor* visitor)   { return visitor->visit(this); }

void AssignStmt::accept(Visitor* visitor)  { visitor->visit(this); }
void PrintStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void IfStmt::accept(Visitor* visitor)      { visitor->visit(this); }
void WhileStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void DoWhileStmt::accept(Visitor* visitor) { visitor->visit(this); }
void BreakStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void ReturnStmt::accept(Visitor* visitor)  { visitor->visit(this); }
void BlockStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void ExprStmt::accept(Visitor* visitor)    { visitor->visit(this); }
void ForStmt::accept(Visitor* visitor)     { visitor->visit(this); }
void ContinueStmt::accept(Visitor* visitor) { visitor->visit(this); }

void VarDec::accept(Visitor* visitor)   { visitor->visit(this); }
void Body::accept(Visitor* visitor)     { visitor->visit(this); }
void FunDec::accept(Visitor* visitor)   { visitor->visit(this); }
void Programa::accept(Visitor* visitor) { visitor->visit(this); }

// =====================================================
// ================== GencodeVisitor ===================
// =====================================================

// Crea el generador de código, que escribe el ensamblador en out.
// Necesita el TypeChecker ya ejecutado: usa los tipos que anotó en cada expresión
// y cuánta pila reservar en cada función.
GencodeVisitor::GencodeVisitor(ostream& out, TypeChecker* checker) : out(out), checker(checker) {}

// Genera el código de todo el programa
void GencodeVisitor::gencode(Programa* program) {
    program->accept(this);
}

// Llama a una función de la biblioteca C con la pila alineada a 16 bytes.
// 'pila' cuenta los pushq pendientes de 8 bytes dentro de la función actual.
void GencodeVisitor::llamar(const string& funcion) {
    bool alinear = pila % 2 != 0;
    if (alinear) out << "subq $8, %rsp" << endl;
    out << "call " << funcion << "@PLT" << endl;
    if (alinear) out << "addq $8, %rsp" << endl;
}

// Conversiones implícitas de C++ ([conv]) sobre el valor que está en %rax
void GencodeVisitor::convertir(Type::Kind desde, Type::Kind hacia) {
    if (desde == hacia || hacia == Type::VOID) return;

    if (hacia == Type::STRING) {
        // string = c: el string pasa a tener solo el carácter c (operator=(char))
        convertir(desde, Type::CHAR);
        out << "pushq %rax" << endl;
        pila++;
        out << "movl $2, %edi" << endl;
        llamar("malloc");
        out << "popq %rcx" << endl;
        pila--;
        out << "movb %cl, (%rax)" << endl;
        out << "movb $0, 1(%rax)" << endl;
        return;
    }

    if (hacia == Type::BOOL) {
        // Cualquier valor distinto de cero es true (NaN también)
        if (Type::esFlotante(desde)) {
            out << "movq %rax, %xmm0" << endl;
            out << "xorpd %xmm1, %xmm1" << endl;
            out << "ucomisd %xmm1, %xmm0" << endl;
            out << "setne %al" << endl;
            out << "setp %cl" << endl;
            out << "orb %cl, %al" << endl;
        }
        else {
            out << "cmpq $0, %rax" << endl;
            out << "setne %al" << endl;
        }
        out << "movzbq %al, %rax" << endl;
        return;
    }

    if (Type::esFlotante(hacia)) {
        if (Type::esFlotante(desde)) {
            // float -> double es exacto; double -> float redondea a precisión float
            if (hacia == Type::FLOAT) {
                out << "movq %rax, %xmm0" << endl;
                out << "cvtsd2ss %xmm0, %xmm0" << endl;
                out << "cvtss2sd %xmm0, %xmm0" << endl;
                out << "movq %xmm0, %rax" << endl;
            }
            return;
        }
        if (hacia == Type::DOUBLE) {
            out << "cvtsi2sdq %rax, %xmm0" << endl;
        }
        else {
            out << "cvtsi2ssq %rax, %xmm0" << endl;     // entero -> float con un solo redondeo
            out << "cvtss2sd %xmm0, %xmm0" << endl;
        }
        out << "movq %xmm0, %rax" << endl;
        return;
    }

    // Hacia un entero: int, long o char
    if (Type::esFlotante(desde)) {
        out << "movq %rax, %xmm0" << endl;
        if (hacia == Type::LONG) {
            out << "cvttsd2siq %xmm0, %rax" << endl;    // trunca hacia cero
        }
        else {
            out << "cvttsd2si %xmm0, %eax" << endl;     // trunca hacia cero (32 bits)
            out << (hacia == Type::INT ? "movslq %eax, %rax" : "movsbq %al, %rax") << endl;
        }
        return;
    }
    if (hacia == Type::INT && desde == Type::LONG) {
        out << "movslq %eax, %rax" << endl;               // se queda con los 32 bits bajos
    }
    else if (hacia == Type::CHAR && desde != Type::BOOL) {
        out << "movsbq %al, %rax" << endl;                // se queda con el byte bajo
    }
    // bool/char/int -> int o long: ya están extendidos a 64 bits
}

// %rax = %rax op %rcx, con ambos operandos ya convertidos al tipo común
void GencodeVisitor::aritmetica(BinaryOp op, Type::Kind tipo) {
    if (Type::esFlotante(tipo)) {
        out << "movq %rax, %xmm0" << endl;
        out << "movq %rcx, %xmm1" << endl;
        switch (op) {
            case PLUS_OP:  out << "addsd %xmm1, %xmm0" << endl; break;
            case MINUS_OP: out << "subsd %xmm1, %xmm0" << endl; break;
            case MUL_OP:   out << "mulsd %xmm1, %xmm0" << endl; break;
            default:       out << "divsd %xmm1, %xmm0" << endl; break;
        }
        // Operar en double y redondear a float da el mismo resultado que operar en float
        if (tipo == Type::FLOAT) {
            out << "cvtsd2ss %xmm0, %xmm0" << endl;
            out << "cvtss2sd %xmm0, %xmm0" << endl;
        }
        out << "movq %xmm0, %rax" << endl;
        return;
    }
    bool es32 = tipo == Type::INT;
    switch (op) {
        case PLUS_OP:  out << "addq %rcx, %rax" << endl; break;
        case MINUS_OP: out << "subq %rcx, %rax" << endl; break;
        case MUL_OP:   out << "imulq %rcx, %rax" << endl; break;
        default:                                   // / y %
            out << "cmpq $0, %rcx" << endl;
            out << "je .Lerror_division_cero" << endl;
            if (es32) {
                out << "cltd" << endl;             // extiende el signo de %eax a %edx
                out << "idivl %ecx" << endl;       // cociente en %eax, resto en %edx
                if (op == MOD_OP) out << "movl %edx, %eax" << endl;
            }
            else {
                out << "cqto" << endl;             // extiende el signo de %rax a %rdx
                out << "idivq %rcx" << endl;       // cociente en %rax, resto en %rdx
                if (op == MOD_OP) out << "movq %rdx, %rax" << endl;
            }
            break;
    }
    // int tiene 32 bits: el resultado se reduce a 32 bits como en C++ (gcc)
    if (es32) out << "movslq %eax, %rax" << endl;
}

// %rax = (%rax op %rcx) ? 1 : 0, con ambos operandos ya convertidos al tipo común
void GencodeVisitor::comparar(BinaryOp op, Type::Kind tipo) {
    if (Type::esFlotante(tipo)) {
        // ucomisd deja CF/ZF como una comparación sin signo y PF=1 si algún operando es NaN.
        // Con NaN toda comparación es falsa salvo !=.
        out << "movq %rax, %xmm0" << endl;
        out << "movq %rcx, %xmm1" << endl;
        switch (op) {
            case LT_OP: out << "ucomisd %xmm0, %xmm1" << endl << "seta %al" << endl; break;   // b > a
            case LE_OP: out << "ucomisd %xmm0, %xmm1" << endl << "setae %al" << endl; break;  // b >= a
            case GT_OP: out << "ucomisd %xmm1, %xmm0" << endl << "seta %al" << endl; break;
            case GE_OP: out << "ucomisd %xmm1, %xmm0" << endl << "setae %al" << endl; break;
            case EQ_OP:
                out << "ucomisd %xmm1, %xmm0" << endl;
                out << "sete %al" << endl << "setnp %cl" << endl << "andb %cl, %al" << endl;
                break;
            default:
                out << "ucomisd %xmm1, %xmm0" << endl;
                out << "setne %al" << endl << "setp %cl" << endl << "orb %cl, %al" << endl;
                break;
        }
        out << "movzbq %al, %rax" << endl;
        return;
    }
    if (tipo == Type::STRING) {
        // Se compara el contenido, como std::string: strcmp(a, b) op 0
        out << "movq %rax, %rdi" << endl;
        out << "movq %rcx, %rsi" << endl;
        llamar("strcmp");
        out << "cmpl $0, %eax" << endl;
    }
    else {
        out << "cmpq %rcx, %rax" << endl;
    }
    switch (op) {
        case LT_OP: out << "setl %al" << endl; break;
        case LE_OP: out << "setle %al" << endl; break;
        case GT_OP: out << "setg %al" << endl; break;
        case GE_OP: out << "setge %al" << endl; break;
        case EQ_OP: out << "sete %al" << endl; break;
        default:    out << "setne %al" << endl; break;
    }
    out << "movzbq %al, %rax" << endl;
}

// Evalúa la condición de un if / while / do / ?: y la convierte a bool (0 o 1)
void GencodeVisitor::condicion(Exp* e) {
    e->accept(this);
    convertir(e->tipoExp, Type::BOOL);
}

// Genera el código de left op right y deja el resultado en %rax
Type GencodeVisitor::visit(BinaryExp* exp) {
    if (exp->op == AND_OP || exp->op == OR_OP) {
        // Evaluación en cortocircuito: el lado derecho solo se evalúa si hace falta
        int id = etiquetas++;
        bool esAnd = exp->op == AND_OP;
        condicion(exp->left);
        out << "cmpq $0, %rax" << endl;
        out << (esAnd ? "je .Llogico_" : "jne .Llogico_") << id << endl;
        condicion(exp->right);
        out << "jmp .Lfinlogico_" << id << endl;
        out << ".Llogico_" << id << ":" << endl;
        out << "movq $" << (esAnd ? 0 : 1) << ", %rax" << endl;
        out << ".Lfinlogico_" << id << ":" << endl;
        return Type::deKind(Type::BOOL);
    }
    Type::Kind k = exp->tipoOperandos;
    exp->left->accept(this);
    convertir(exp->left->tipoExp, k);
    out << "pushq %rax" << endl;
    pila++;
    exp->right->accept(this);
    convertir(exp->right->tipoExp, k);
    out << "movq %rax, %rcx" << endl;
    out << "popq %rax" << endl;
    pila--;
    switch (exp->op) {
        case PLUS_OP: case MINUS_OP: case MUL_OP: case DIV_OP: case MOD_OP:
            aritmetica(exp->op, k);
            break;
        default:
            comparar(exp->op, k);
            break;
    }
    return Type::deKind(exp->tipoExp);
}

// -e: se promueve el operando y se cambia el signo.  !e: e convertida a bool, invertida.
Type GencodeVisitor::visit(UnaryExp* exp) {
    if (exp->op == NOT_OP) {
        condicion(exp->e);
        out << "xorq $1, %rax" << endl;
        return Type::deKind(Type::BOOL);
    }
    exp->e->accept(this);
    convertir(exp->e->tipoExp, exp->tipoExp);
    switch (exp->tipoExp) {
        case Type::FLOAT: case Type::DOUBLE:
            out << "btcq $63, %rax" << endl;       // invierte el bit de signo (también -0.0)
            break;
        case Type::INT:
            out << "negq %rax" << endl;
            out << "movslq %eax, %rax" << endl;
            break;
        default:
            out << "negq %rax" << endl;
            break;
    }
    return Type::deKind(exp->tipoExp);
}

// Carga el literal en %rax
Type GencodeVisitor::visit(NumberExp* exp) {
    switch (exp->tipo) {
        case Type::DOUBLE: {
            uint64_t bits;
            memcpy(&bits, &exp->valorFloat, sizeof bits);      // los 64 bits del double
            out << "movabsq $" << bits << ", %rax" << endl;
            break;
        }
        case Type::STRING:
            out << "leaq .Lstr_" << cadenas.size() << "(%rip), %rax" << endl;
            cadenas.push_back(exp->texto);                      // se emite al final del programa
            break;
        case Type::LONG:
            out << "movabsq $" << exp->value << ", %rax" << endl;
            break;
        default:
            out << "movq $" << exp->value << ", %rax" << endl;
            break;
    }
    return Type::deKind(exp->tipoExp);
}

// Carga el valor de la variable desde la pila en %rax
Type GencodeVisitor::visit(IdExp* exp) {
    out << "movq " << posicion.lookup(exp->value) * -TAM_VARIABLE << "(%rbp), %rax" << endl;
    return Type::deKind(exp->tipoExp);
}

// Deja en %rax la dirección de una variable (sin índices) o de un elemento / fila de un arreglo.
// La variable de un arreglo guarda un puntero a su primer elemento; el elemento [i][j]
// está en puntero + i * paso(0) + j * paso(1).
void GencodeVisitor::direccion(const string& nombre, const vector<Exp*>& indices) {
    int offset = posicion.lookup(nombre) * -TAM_VARIABLE;
    if (indices.empty()) {
        out << "leaq " << offset << "(%rbp), %rax" << endl;
        return;
    }
    vector<int> dims = tipos.lookup(nombre).dims;
    out << "movq " << offset << "(%rbp), %rax" << endl;
    out << "pushq %rax" << endl;
    pila++;
    for (size_t k = 0; k < indices.size(); k++) {
        indices[k]->accept(this);
        convertir(indices[k]->tipoExp, Type::LONG);
        out << "imulq $" << pasoIndice(dims, k) << ", %rax" << endl;
        out << "addq %rax, (%rsp)" << endl;
    }
    out << "popq %rax" << endl;
    pila--;
}

// a[i][j]: carga el elemento; con menos índices que dimensiones deja la dirección de la fila
Type GencodeVisitor::visit(ArrayExp* exp) {
    direccion(exp->nombre, exp->indices);
    if (exp->dimsExp.empty()) out << "movq (%rax), %rax" << endl;
    return Type::deKind(exp->tipoExp);
}

// ++x / x++ / --x / x--: suma o resta 1 en la variable o elemento.
// El prefijo devuelve el valor nuevo y el sufijo el anterior.
Type GencodeVisitor::visit(IncDecExp* exp) {
    Type::Kind k = exp->tipoExp;
    direccion(exp->nombre, exp->indices);
    out << "movq %rax, %rsi" << endl;              // dirección
    out << "movq (%rsi), %rax" << endl;
    out << "movq %rax, %rcx" << endl;              // valor anterior
    if (Type::esFlotante(k)) {
        double uno = 1.0;
        uint64_t bits;
        memcpy(&bits, &uno, sizeof bits);
        out << "movq %rax, %xmm0" << endl;
        out << "movabsq $" << bits << ", %rdx" << endl;
        out << "movq %rdx, %xmm1" << endl;
        out << (exp->incremento ? "addsd" : "subsd") << " %xmm1, %xmm0" << endl;
        if (k == Type::FLOAT) {
            out << "cvtsd2ss %xmm0, %xmm0" << endl;
            out << "cvtss2sd %xmm0, %xmm0" << endl;
        }
        out << "movq %xmm0, %rax" << endl;
    }
    else {
        out << (exp->incremento ? "addq $1, %rax" : "subq $1, %rax") << endl;
        if (k == Type::INT) out << "movslq %eax, %rax" << endl;
        else if (k == Type::CHAR) out << "movsbq %al, %rax" << endl;
    }
    out << "movq %rax, (%rsi)" << endl;
    if (!exp->prefijo) out << "movq %rcx, %rax" << endl;
    return Type::deKind(k);
}

// Genera el código del ?: deja en %rax el valor de la rama elegida, convertido al tipo común
Type GencodeVisitor::visit(IfExp* exp) {
    int id = etiquetas++;
    condicion(exp->condicion);
    out << "cmpq $0, %rax" << endl;
    out << "je .Lifexp_else_" << id << endl;
    exp->thenExp->accept(this);
    convertir(exp->thenExp->tipoExp, exp->tipoExp);
    out << "jmp .Lifexp_end_" << id << endl;
    out << ".Lifexp_else_" << id << ":" << endl;
    exp->elseExp->accept(this);
    convertir(exp->elseExp->tipoExp, exp->tipoExp);
    out << ".Lifexp_end_" << id << ":" << endl;
    return Type::deKind(exp->tipoExp);
}

// Llamada: evalúa los argumentos, los pasa en registros y llama a la función.
// El resultado queda en %rax.
Type GencodeVisitor::visit(FCallExp* exp) {
    if (!exp->predefinida.empty()) {
        // std::max(a, b) = (a < b) ? b : a      std::min(a, b) = (b < a) ? b : a
        Type::Kind k = exp->tipoExp;
        exp->argumentos[0]->accept(this);
        out << "pushq %rax" << endl;
        pila++;
        exp->argumentos[1]->accept(this);
        out << "pushq %rax" << endl;
        pila++;
        bool esMax = exp->predefinida == "max";
        out << "movq " << (esMax ? "8(%rsp)" : "(%rsp)") << ", %rax" << endl;
        out << "movq " << (esMax ? "(%rsp)" : "8(%rsp)") << ", %rcx" << endl;
        comparar(LT_OP, k);
        out << "popq %rcx" << endl;                // b
        out << "popq %rdx" << endl;                // a
        pila -= 2;
        out << "testq %rax, %rax" << endl;
        out << "cmovne %rcx, %rdx" << endl;
        out << "movq %rdx, %rax" << endl;
        return Type::deKind(k);
    }

    FunDec* f = funciones[exp->nombre];
    for (size_t i = 0; i < exp->argumentos.size(); i++) {
        exp->argumentos[i]->accept(this);
        convertir(exp->argumentos[i]->tipoExp, kindDe(f->parametros_tipos[i]));   // al tipo del parámetro
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
    out << "call " << etiquetaFuncion(exp->nombre) << endl;
    if (alinear) out << "addq $8, %rsp" << endl;
    return Type::deKind(exp->tipoExp);
}

// Evalúa la expresión y la guarda en la variable o en el elemento del arreglo
void GencodeVisitor::visit(AssignStmt* stm) {
    Type destino = tipos.lookup(stm->id);            // tipo con el que se declaró la variable
    stm->e->accept(this);
    convertir(stm->e->tipoExp, destino.kind);
    if (stm->indices.empty()) {
        out << "movq %rax, " << posicion.lookup(stm->id) * -TAM_VARIABLE << "(%rbp)" << endl;
        return;
    }
    // a[i] = e: en C++17 el valor se evalúa antes que la posición
    out << "pushq %rax" << endl;
    pila++;
    direccion(stm->id, stm->indices);
    out << "popq %rcx" << endl;
    pila--;
    out << "movq %rcx, (%rax)" << endl;
}

// Imprime cada operando de cout con el formato de su tipo; endl imprime '\n'
void GencodeVisitor::visit(PrintStmt* stm) {
    for (Exp* e : stm->expresiones) {
        if (!e) {
            out << "leaq print_fmt_nl(%rip), %rdi" << endl;
            out << "movl $0, %eax" << endl;
            llamar("printf");
            continue;
        }
        e->accept(this);
        Type::Kind k = e->tipoExp;
        if (Type::esFlotante(k)) {
            // cout imprime float y double con 6 cifras significativas, igual que %g
            out << "movq %rax, %xmm0" << endl;
            out << "leaq print_fmt_float(%rip), %rdi" << endl;
            out << "movl $1, %eax" << endl;
        }
        else {
            const char* formato = k == Type::CHAR ? "print_fmt_char"
                                : k == Type::STRING ? "print_fmt_str" : "print_fmt";
            out << "movq %rax, %rsi" << endl;
            out << "leaq " << formato << "(%rip), %rdi" << endl;
            out << "movl $0, %eax" << endl;
        }
        llamar("printf");
    }
}

// Genera el código del if: si la condición es 0 salta a la rama else.
// Cada rama es un ámbito propio, como en C++.
void GencodeVisitor::visit(IfStmt* stm) {
    int id = etiquetas++;
    condicion(stm->condicion);
    out << "cmpq $0, %rax" << endl;
    out << "je .Lelse_" << id << endl;
    posicion.add_level(); tipos.add_level();
    for (Stmt* s : stm->thenList) s->accept(this);
    posicion.remove_level(); tipos.remove_level();
    out << "jmp .Lendif_" << id << endl;
    out << ".Lelse_" << id << ":" << endl;
    posicion.add_level(); tipos.add_level();
    for (Stmt* s : stm->elseList) s->accept(this);
    posicion.remove_level(); tipos.remove_level();
    out << ".Lendif_" << id << ":" << endl;
}

// Genera el código del while: evalúa la condición al inicio de cada vuelta
void GencodeVisitor::visit(WhileStmt* stm) {
    int id = etiquetas++;
    string fin = ".Lendwhile_" + to_string(id);
    out << ".Lwhile_" << id << ":" << endl;
    condicion(stm->condicion);
    out << "cmpq $0, %rax" << endl;
    out << "je " << fin << endl;
    finDeBucle.push_back(fin);
    siguienteVuelta.push_back(".Lwhile_" + to_string(id));
    posicion.add_level(); tipos.add_level();
    for (Stmt* s : stm->body) s->accept(this);
    posicion.remove_level(); tipos.remove_level();
    finDeBucle.pop_back();
    siguienteVuelta.pop_back();
    out << "jmp .Lwhile_" << id << endl;
    out << fin << ":" << endl;
}

// Genera el código del do-while: ejecuta el cuerpo y repite mientras la condición no sea 0
void GencodeVisitor::visit(DoWhileStmt* stm) {
    int id = etiquetas++;
    string fin = ".Lenddowhile_" + to_string(id);
    out << ".Ldowhile_" << id << ":" << endl;
    finDeBucle.push_back(fin);
    siguienteVuelta.push_back(".Ldocond_" + to_string(id));
    posicion.add_level(); tipos.add_level();
    for (Stmt* s : stm->body) s->accept(this);
    posicion.remove_level(); tipos.remove_level();
    finDeBucle.pop_back();
    siguienteVuelta.pop_back();
    out << ".Ldocond_" << id << ":" << endl;     // continue vuelve a evaluar la condición
    condicion(stm->condicion);
    out << "cmpq $0, %rax" << endl;
    out << "jne .Ldowhile_" << id << endl;
    out << fin << ":" << endl;
}

// Las sentencias del cuerpo de un for. Si el cuerpo es un bloque { }, sus declaraciones
// quedan en el mismo ámbito que las del init, como en C++ (no se puede redeclarar i).
void GencodeVisitor::cuerpoDeFor(ForStmt* stm) {
    for (Stmt* s : stm->body) {
        if (BlockStmt* b = dynamic_cast<BlockStmt*>(s)) {
            for (Stmt* t : b->sentencias) t->accept(this);
        }
        else {
            s->accept(this);
        }
    }
}

// Genera el código del for:
//   init; inicio: si !cond salta a fin; cuerpo; siguiente: actualizacion; salta a inicio; fin:
void GencodeVisitor::visit(ForStmt* stm) {
    int id = etiquetas++;
    string fin = ".Lendfor_" + to_string(id);
    string siguiente = ".Lforsig_" + to_string(id);
    posicion.add_level(); tipos.add_level();
    if (stm->init) stm->init->accept(this);
    out << ".Lfor_" << id << ":" << endl;
    if (stm->condicion) {
        condicion(stm->condicion);
        out << "cmpq $0, %rax" << endl;
        out << "je " << fin << endl;
    }
    finDeBucle.push_back(fin);
    siguienteVuelta.push_back(siguiente);
    cuerpoDeFor(stm);
    finDeBucle.pop_back();
    siguienteVuelta.pop_back();
    out << siguiente << ":" << endl;
    if (stm->actualizacion) stm->actualizacion->accept(this);
    out << "jmp .Lfor_" << id << endl;
    out << fin << ":" << endl;
    posicion.remove_level(); tipos.remove_level();
}

// Genera el código del break: salta a la salida del bucle más interno
void GencodeVisitor::visit(BreakStmt* stm) {
    out << "jmp " << finDeBucle.back() << endl;
}

// Genera el código del continue: salta a la siguiente vuelta del bucle más interno
void GencodeVisitor::visit(ContinueStmt* stm) {
    out << "jmp " << siguienteVuelta.back() << endl;
}

// Genera el código del return: deja el valor en %rax y salta al epílogo de la función
void GencodeVisitor::visit(ReturnStmt* stm) {
    if (stm->exp) {
        stm->exp->accept(this);
        convertir(stm->exp->tipoExp, kindDe(funciones[funcionActual]->tipo));   // al tipo de la función
    }
    out << "jmp .Lfin_" << funcionActual << endl;
}

// Bloque { ... }: sus declaraciones solo son visibles dentro
void GencodeVisitor::visit(BlockStmt* stm) {
    posicion.add_level(); tipos.add_level();
    for (Stmt* s : stm->sentencias) s->accept(this);
    posicion.remove_level(); tipos.remove_level();
}

// e;  se evalúa y se descarta el resultado
void GencodeVisitor::visit(ExprStmt* stm) {
    stm->e->accept(this);
}

// Declara cada variable en el nivel actual: le asigna una posición en la pila y su tipo.
// Un arreglo de n elementos ocupa n posiciones seguidas más una con el puntero al primero.
void GencodeVisitor::visit(VarDec* dec) {
    Type destino = tipoDe(dec->tipo);
    for (size_t i = 0; i < dec->variables.size(); i++) {
        const string& v = dec->variables[i];
        if (!dec->dimensiones[i].empty()) {
            Type arreglo = destino;
            arreglo.dims = dec->dimensiones[i];
            long long n = totalElementos(arreglo.dims);
            long long primero = contador + n - 1;   // el elemento 0 queda en la dirección más baja
            long long puntero = contador + n;
            contador += n + 1;
            posicion.add_var(v, puntero);
            tipos.add_var(v, arreglo);
            out << "leaq " << primero * -TAM_VARIABLE << "(%rbp), %rax" << endl;
            out << "movq %rax, " << puntero * -TAM_VARIABLE << "(%rbp)" << endl;
            // Todos los elementos empiezan en 0 ("" si son string)
            out << "movq %rax, %rdi" << endl;
            out << "movq $" << n << ", %rcx" << endl;
            if (destino.isString()) out << "leaq .Lcadena_vacia(%rip), %rax" << endl;
            else out << "movq $0, %rax" << endl;
            out << "rep stosq" << endl;
            // Los elementos que aparecen en { ... }
            for (long long k = 0; k < n; k++) {
                Exp* e = dec->listas[i][k];
                if (!e) continue;
                e->accept(this);
                convertir(e->tipoExp, destino.kind);
                out << "movq %rax, " << (primero - k) * -TAM_VARIABLE << "(%rbp)" << endl;
            }
            continue;
        }
        posicion.add_var(v, contador++);
        tipos.add_var(v, destino);
        if (dec->inicializadores[i]) {
            dec->inicializadores[i]->accept(this);
            convertir(dec->inicializadores[i]->tipoExp, destino.kind);
        } else if (destino.isString()) {
            out << "leaq .Lcadena_vacia(%rip), %rax" << endl;   // un string sin inicializar es ""
        } else {
            out << "movq $0, %rax" << endl;
        }
        out << "movq %rax, " << posicion.lookup(v) * -TAM_VARIABLE << "(%rbp)" << endl;
    }
}

// Procesa las declaraciones y las demás sentencias en su orden original
void GencodeVisitor::visit(Body* body) {
    for (Stmt* s : body->sentencias) s->accept(this);
}

// Genera una función: prólogo, copia de parámetros a la pila, cuerpo y epílogo.
// Los prototipos no generan código.
void GencodeVisitor::visit(FunDec* fun) {
    if (!fun->cuerpo) return;
    funcionActual = fun->nombre;
    contador = 1;
    posicion.add_level();
    tipos.add_level();

    // Espacio en la pila según lo que contó el TypeChecker
    int espacio = checker->espacioPila(fun->nombre);

    out << etiquetaFuncion(fun->nombre) << ":" << endl;
    out << "pushq %rbp" << endl;
    out << "movq %rsp, %rbp" << endl;
    if (espacio > 0) out << "subq $" << espacio << ", %rsp" << endl;

    // Los parámetros llegan en registros: se guardan en la pila como variables locales
    for (size_t i = 0; i < fun->parametros_id.size(); i++) {
        const string& p = fun->parametros_id[i];
        int pos = contador++;
        if (!p.empty()) {
            posicion.add_var(p, pos);
            tipos.add_var(p, tipoDe(fun->parametros_tipos[i]));
        }
        out << "movq " << REG_ARGS[i] << ", " << pos * -TAM_VARIABLE << "(%rbp)" << endl;
    }

    fun->cuerpo->accept(this);

    out << "movq $0, %rax" << endl;            // si no hubo return, devuelve 0
    out << ".Lfin_" << fun->nombre << ":" << endl;
    out << "leave" << endl;
    out << "ret" << endl;

    posicion.remove_level();
    tipos.remove_level();
}

// Genera el programa completo: sección de datos y el código de cada función
void GencodeVisitor::visit(Programa* program) {
    for (FunDec* f : program->funciones) {
        if (!funciones.count(f->nombre)) funciones[f->nombre] = f;
    }

    out << ".data" << endl;
    out << "print_fmt:.string \"%ld\"" << endl;
    out << "print_fmt_char:.string \"%c\"" << endl;
    out << "print_fmt_float:.string \"%g\"" << endl;
    out << "print_fmt_str:.string \"%s\"" << endl;
    out << "print_fmt_nl:.string \"\\n\"" << endl;
    out << ".Lcadena_vacia:.string \"\"" << endl;
    out << ".Lmensaje_division_cero:.string \"Error de ejecucion: division entre cero.\\n\"" << endl;
    out << ".text" << endl;
    out << ".globl main" << endl;
    for (FunDec* f : program->funciones) f->accept(this);
    // Se llega por salto desde cualquier expresión. No retorna y realinea la
    // pila, incluso si había operandos pendientes o llamadas recursivas.
    out << ".Lerror_division_cero:" << endl;
    out << "andq $-16, %rsp" << endl;
    out << "leaq .Lmensaje_division_cero(%rip), %rdi" << endl;
    out << "movq stderr(%rip), %rsi" << endl;
    out << "call fputs@PLT" << endl;
    out << "movl $1, %edi" << endl;
    out << "call exit@PLT" << endl;
    out << ".section .rodata" << endl;
    for (size_t i = 0; i < cadenas.size(); i++) {
        out << ".Lstr_" << i << ":.string \"" << escaparAsm(cadenas[i]) << "\"" << endl;
    }
    out << ".section .note.GNU-stack,\"\",@progbits" << endl;
}

// =====================================================
// ==================== TypeChecker ====================
// =====================================================

// Lanza un error de tipos con el mensaje dado
static void error(const string& msg) {
    throw runtime_error(msg);
}

// Conversión implícita (inicialización, argumento, return): entre tipos aritméticos
// siempre se puede (como en C++); string solo desde string.
static bool convertible(Type::Kind destino, Type::Kind origen) {
    if (destino == origen) return true;
    return Type::esAritmetico(destino) && Type::esAritmetico(origen);
}

// Anota el tipo de la expresión (y sus dimensiones si es un arreglo) y lo devuelve
static Type anotar(Exp* e, Type::Kind k, const vector<int>& dims = {}) {
    e->tipoExp = k;
    e->dimsExp = dims;
    Type t = Type::deKind(k);
    t.dims = dims;
    return t;
}

// Los enteros (bool, char, int, long) sirven como índice y para %
static bool esEntero(const Type& t) {
    return t.isNumeric() && !t.isFloat();
}

// Verifica todo el programa
void TypeChecker::check(Programa* program) {
    program->accept(this);
}

// Bytes que debe reservar la función en la pila: 8 por variable,
// redondeado a múltiplo de 16 para mantener la pila alineada
int TypeChecker::espacioPila(const string& funcion) {
    int bytes = numVariables[funcion] * TAM_VARIABLE;
    return ((bytes + 15) / 16) * 16;
}

// La condición de una sentencia debe poder convertirse a bool (cualquier tipo aritmético)
void TypeChecker::esperarBool(Exp* condicion, const string& donde) {
    Type c = condicion->accept(this);
    if (!c.isNumeric()) {
        error("la condición del " + donde + " no se puede convertir a bool (es " + c.kindName() + ")");
    }
}

// ---------- Expresiones ----------

// Aritméticos: conversiones aritméticas usuales. Comparaciones y lógicos: devuelven bool.
Type TypeChecker::visit(BinaryExp* exp) {
    Type l = exp->left->accept(this);
    Type r = exp->right->accept(this);
    string op = Exp::binopToChar(exp->op);
    string operandos = l.kindName() + " " + op + " " + r.kindName();
    bool comparacion = exp->op != PLUS_OP && exp->op != MINUS_OP && exp->op != MUL_OP
                    && exp->op != DIV_OP && exp->op != MOD_OP;

    if (exp->op == AND_OP || exp->op == OR_OP) {
        if (!l.isNumeric() || !r.isNumeric()) {
            error("el operador '" + op + "' necesita operandos convertibles a bool, pero recibió " + operandos);
        }
        return anotar(exp, Type::BOOL);
    }
    if (exp->op == MOD_OP && (!esEntero(l) || !esEntero(r))) {
        error("el operador '%' necesita operandos enteros, pero recibió " + operandos);
    }
    if (l.isNumeric() && r.isNumeric()) {
        exp->tipoOperandos = Type::comun(l.kind, r.kind);
        return anotar(exp, comparacion ? Type::BOOL : exp->tipoOperandos);
    }
    // std::string se compara por contenido
    if (comparacion && l.isString() && r.isString()) {
        exp->tipoOperandos = Type::STRING;
        return anotar(exp, Type::BOOL);
    }
    error("el operador '" + op + "' no se puede aplicar a " + operandos);
    return Type();
}

// -e: e aritmético; el resultado es e promovido (bool y char pasan a int)
// !e: e convertible a bool; el resultado es bool
Type TypeChecker::visit(UnaryExp* exp) {
    Type t = exp->e->accept(this);
    string op = exp->op == NOT_OP ? "!" : "-";
    if (!t.isNumeric()) {
        error("el operador '" + op + "' no se puede aplicar a " + t.kindName());
    }
    if (exp->op == NOT_OP) return anotar(exp, Type::BOOL);
    return anotar(exp, Type::promover(t.kind));
}

// Verifica nombre[i]...[j] (o solo nombre) como destino de una asignación o de ++ / --
// y devuelve el tipo del elemento
Type TypeChecker::destino(const string& nombre, const vector<Exp*>& indices, const string& accion) {
    if (!tipos.check(nombre)) {
        error("variable no declarada: " + nombre);
    }
    Type t = tipos.lookup(nombre);
    if (indices.size() > t.dims.size()) {
        error(nombre + " no tiene tantas dimensiones (es " + t.kindName() + ")");
    }
    for (Exp* i : indices) {
        Type ti = i->accept(this);
        if (!esEntero(ti)) error("el índice de " + nombre + " debe ser entero, pero es " + ti.kindName());
    }
    if (indices.size() < t.dims.size()) {
        error("no se puede " + accion + " un arreglo completo (" + nombre + " es " + t.kindName() + ")");
    }
    return Type::deKind(t.kind);
}

// a[i][j]: a es un arreglo, cada índice es entero. Con menos índices que dimensiones
// el resultado es una fila (un arreglo)
Type TypeChecker::visit(ArrayExp* exp) {
    if (!tipos.check(exp->nombre)) {
        error("variable no declarada: " + exp->nombre);
    }
    Type t = tipos.lookup(exp->nombre);
    if (!t.isArray()) {
        error(exp->nombre + " no es un arreglo");
    }
    if (exp->indices.size() > t.dims.size()) {
        error(exp->nombre + " no tiene tantas dimensiones (es " + t.kindName() + ")");
    }
    for (Exp* i : exp->indices) {
        Type ti = i->accept(this);
        if (!esEntero(ti)) error("el índice de " + exp->nombre + " debe ser entero, pero es " + ti.kindName());
    }
    vector<int> resto(t.dims.begin() + exp->indices.size(), t.dims.end());
    return anotar(exp, t.kind, resto);
}

// ++x / x-- : x es una variable o elemento numérico. En C++17 no se permite sobre bool.
Type TypeChecker::visit(IncDecExp* exp) {
    string op = exp->incremento ? "++" : "--";
    Type t = destino(exp->nombre, exp->indices, "aplicar '" + op + "' a");
    if (!t.isNumeric() || t.isBool()) {
        error("el operador '" + op + "' no se puede aplicar a " + t.kindName());
    }
    return anotar(exp, t.kind);
}

// Tipo del literal
Type TypeChecker::visit(NumberExp* exp) {
    return anotar(exp, exp->esBool ? Type::BOOL : exp->tipo);
}

// Una variable tiene el tipo con el que fue declarada
Type TypeChecker::visit(IdExp* exp) {
    if (!tipos.check(exp->value)) {
        error("variable no declarada: " + exp->value);
    }
    Type t = tipos.lookup(exp->value);
    return anotar(exp, t.kind, t.dims);
}

// c ? e1 : e2: c convertible a bool; si las ramas son aritméticas de distinto tipo
// se aplica la conversión aritmética usual
Type TypeChecker::visit(IfExp* exp) {
    Type c = exp->condicion->accept(this);
    if (!c.isNumeric()) {
        error("la condición de ?: no se puede convertir a bool (es " + c.kindName() + ")");
    }
    Type t = exp->thenExp->accept(this);
    Type e = exp->elseExp->accept(this);
    if (t.isArray() || e.isArray()) {
        error("las ramas de ?: no pueden ser arreglos");
    }
    if (t.kind == e.kind) return anotar(exp, t.kind);
    if (t.isNumeric() && e.isNumeric()) return anotar(exp, Type::comun(t.kind, e.kind));
    error("las ramas de ?: tienen tipos incompatibles: " + t.kindName() + " y " + e.kindName());
    return Type();
}

// f(a1, ..., an): la función ya está declarada (como en C++, antes de usarse),
// recibe n argumentos y cada uno es convertible al tipo de su parámetro.
// max / min sin declarar se resuelven a std::max / std::min.
Type TypeChecker::visit(FCallExp* exp) {
    const string& nombre = exp->nombre;
    bool calificado = nombre.rfind("std::", 0) == 0;

    if (!calificado && tipos.check(nombre)) {
        error("'" + nombre + "' es una variable y no se puede llamar como función");
    }
    if (!calificado && funciones.count(nombre)) {
        FunDec* f = funciones[nombre];
        llamadas.insert(nombre);
        if (exp->argumentos.size() != f->parametros_tipos.size()) {
            error("la función " + f->nombre + " espera " + to_string(f->parametros_tipos.size())
                  + " argumento(s), pero recibió " + to_string(exp->argumentos.size()));
        }
        for (size_t i = 0; i < exp->argumentos.size(); i++) {
            Type t = exp->argumentos[i]->accept(this);
            Type p = tipoDe(f->parametros_tipos[i]);
            bool ok;
            if (p.isArray() || t.isArray()) {
                // Un arreglo se pasa como puntero a su primer elemento: mismo tipo de elemento
                // y mismas dimensiones salvo la primera
                ok = p.isArray() && t.isArray() && p.kind == t.kind && p.dims.size() == t.dims.size()
                     && equal(p.dims.begin() + 1, p.dims.end(), t.dims.begin() + 1);
            }
            else {
                ok = convertible(p.kind, t.kind);
            }
            if (!ok) {
                error("el argumento " + to_string(i + 1) + " de " + f->nombre + " debe ser "
                      + f->parametros_tipos[i] + ", pero es " + t.kindName());
            }
        }
        return anotar(exp, kindDe(f->tipo));
    }

    string base = calificado ? nombre.substr(5) : nombre;
    if ((base == "max" || base == "min") && (calificado || exp->usingStd)) {
        // template<class T> const T& max(const T& a, const T& b): ambos del mismo tipo T
        if (exp->argumentos.size() != 2) {
            error(nombre + " necesita 2 argumentos, pero recibió " + to_string(exp->argumentos.size()));
        }
        Type a = exp->argumentos[0]->accept(this);
        Type b = exp->argumentos[1]->accept(this);
        if (a.isArray() || b.isArray()) {
            error("los argumentos de " + nombre + " no pueden ser arreglos");
        }
        if (a.kind != b.kind) {
            error("los argumentos de " + nombre + " deben ser del mismo tipo, pero son "
                  + a.kindName() + " y " + b.kindName());
        }
        exp->predefinida = base;
        return anotar(exp, a.kind);
    }
    error("función no declarada: " + nombre);
    return Type();
}

// ---------- Sentencias ----------

// x = e  o  a[i] = e: destino declarado y e convertible a su tipo.
// A un string también se le puede asignar un valor aritmético (std::string::operator=(char)).
void TypeChecker::visit(AssignStmt* stm) {
    Type t = stm->e->accept(this);
    Type declarado = destino(stm->id, stm->indices, "asignar a");
    if (t.isArray()) {
        error("no se puede asignar un arreglo");
    }
    bool ok = convertible(declarado.kind, t.kind) || (declarado.isString() && t.isNumeric());
    if (!ok) {
        error("no se puede asignar " + t.kindName() + " a la variable "
              + declarado.kindName() + " " + stm->id);
    }
}

// Cada operando de cout debe tener un valor que pueda imprimirse (nullptr es endl)
void TypeChecker::visit(PrintStmt* stm) {
    for (Exp* expresion : stm->expresiones) {
        if (!expresion) continue;
        Type t = expresion->accept(this);
        if (t.isVoid()) {
            error("cout necesita expresiones con valor");
        }
        if (t.isArray()) {
            error("cout no puede imprimir un arreglo completo");
        }
    }
}

// if: condición convertible a bool; cada rama tiene su propio ámbito
void TypeChecker::visit(IfStmt* stm) {
    esperarBool(stm->condicion, "if");
    tipos.add_level();
    for (Stmt* s : stm->thenList) s->accept(this);
    tipos.remove_level();
    tipos.add_level();
    for (Stmt* s : stm->elseList) s->accept(this);
    tipos.remove_level();
}

// while: condición convertible a bool y se verifica el cuerpo
void TypeChecker::visit(WhileStmt* stm) {
    esperarBool(stm->condicion, "while");
    bucles++;
    tipos.add_level();
    for (Stmt* s : stm->body) s->accept(this);
    tipos.remove_level();
    bucles--;
}

// do-while: se verifica el cuerpo y la condición debe ser convertible a bool
void TypeChecker::visit(DoWhileStmt* stm) {
    bucles++;
    tipos.add_level();
    for (Stmt* s : stm->body) s->accept(this);
    tipos.remove_level();
    bucles--;
    esperarBool(stm->condicion, "do-while");
}

// for: init, condición convertible a bool y actualización comparten un ámbito con el
// cuerpo; si el cuerpo es un bloque { }, no puede redeclarar las variables del init
void TypeChecker::visit(ForStmt* stm) {
    tipos.add_level();
    if (stm->init) stm->init->accept(this);
    if (stm->condicion) esperarBool(stm->condicion, "for");
    if (stm->actualizacion) stm->actualizacion->accept(this);
    bucles++;
    for (Stmt* s : stm->body) {
        if (BlockStmt* b = dynamic_cast<BlockStmt*>(s)) {
            for (Stmt* t : b->sentencias) t->accept(this);
        }
        else {
            s->accept(this);
        }
    }
    bucles--;
    tipos.remove_level();
}

// break: solo dentro de un bucle
void TypeChecker::visit(BreakStmt* stm) {
    if (bucles == 0) {
        error("'break' fuera de un bucle");
    }
}

// continue: solo dentro de un bucle
void TypeChecker::visit(ContinueStmt* stm) {
    if (bucles == 0) {
        error("'continue' fuera de un bucle");
    }
}

// return [e]: en una función void no hay valor (o es una llamada void);
// en las demás, e convertible al tipo de la función
void TypeChecker::visit(ReturnStmt* stm) {
    bool esVoid = funcionActual->tipo == "void";
    if (!stm->exp) {
        if (!esVoid) {
            error("'return;' sin valor en la función " + funcionActual->nombre
                  + ", que debe devolver " + funcionActual->tipo);
        }
        return;
    }
    Type t = stm->exp->accept(this);
    if (esVoid) {
        if (!t.isVoid()) {
            error("la función void " + funcionActual->nombre + " no puede devolver un valor");
        }
        return;
    }
    if (t.isArray() || !convertible(kindDe(funcionActual->tipo), t.kind)) {
        error("la función " + funcionActual->nombre + " debe devolver "
              + funcionActual->tipo + ", pero return devuelve " + t.kindName());
    }
}

// { ... }: abre un ámbito
void TypeChecker::visit(BlockStmt* stm) {
    tipos.add_level();
    for (Stmt* s : stm->sentencias) s->accept(this);
    tipos.remove_level();
}

// e;
void TypeChecker::visit(ExprStmt* stm) {
    stm->e->accept(this);
}

// ---------- Declaraciones y programa ----------

// tipo ids: sin variables repetidas en el mismo ámbito. Como en C++, la variable
// ya es visible dentro de su propio inicializador.
void TypeChecker::visit(VarDec* dec) {
    Type::Kind k = kindDe(dec->tipo);
    if (k == Type::VOID) {
        error("una variable no puede ser de tipo " + dec->tipo);
    }
    for (size_t i = 0; i < dec->variables.size(); i++) {
        const string& v = dec->variables[i];
        if (tipos.check_local(v)) {
            error("variable declarada dos veces: " + v);
        }
        if (!dec->dimensiones[i].empty()) {
            // Arreglo: n elementos más el puntero al primero
            Type arreglo = Type::deKind(k);
            arreglo.dims = dec->dimensiones[i];
            tipos.add_var(v, arreglo);
            numVariables[funcionActual->nombre] += totalElementos(arreglo.dims) + 1;
            for (Exp* e : dec->listas[i]) {
                if (!e) continue;
                Type t = e->accept(this);
                if (t.isArray() || !convertible(k, t.kind)) {
                    error("no se puede inicializar un elemento de " + v + " (" + dec->tipo
                          + ") con un valor " + t.kindName());
                }
            }
            continue;
        }
        tipos.add_var(v, Type::deKind(k));
        numVariables[funcionActual->nombre]++;      // una variable local más
        if (dec->inicializadores[i]) {
            Type t = dec->inicializadores[i]->accept(this);
            if (t.isArray() || !convertible(k, t.kind)) {
                error("no se puede inicializar la variable " + dec->tipo + " " + v
                      + " con un valor " + t.kindName());
            }
        }
    }
}

// Las variables se registran cuando se alcanza su declaración, no antes
void TypeChecker::visit(Body* body) {
    for (Stmt* s : body->sentencias) s->accept(this);
}

// Una función abre su propio nivel con sus parámetros y variables locales
// (en C++ los parámetros y el bloque de la función comparten ámbito)
void TypeChecker::visit(FunDec* fun) {
    funcionActual = fun;
    numVariables[fun->nombre] = fun->parametros_id.size();   // los parámetros también ocupan pila
    tipos.add_level();
    for (size_t i = 0; i < fun->parametros_id.size(); i++) {
        const string& p = fun->parametros_id[i];
        if (p.empty()) continue;
        if (tipos.check_local(p)) {
            error("parámetro repetido en " + fun->nombre + ": " + p);
        }
        tipos.add_var(p, tipoDe(fun->parametros_tipos[i]));
    }
    fun->cuerpo->accept(this);
    tipos.remove_level();
    funcionActual = nullptr;
}

// Registra la declaración (prototipo o definición) de una función y la compara con
// las anteriores del mismo nombre
void TypeChecker::registrar(FunDec* f) {
    if (f->parametros_tipos.size() > 6) {
        error("la función " + f->nombre + " tiene más de 6 parámetros");
    }
    if (funciones.count(f->nombre)) {
        FunDec* previa = funciones[f->nombre];
        if (previa->parametros_tipos != f->parametros_tipos) {
            error("sobrecarga de funciones no soportada: " + f->nombre);
        }
        if (previa->tipo != f->tipo) {
            error("la función " + f->nombre + " se redeclaró con otro tipo de retorno");
        }
    }
    else {
        funciones[f->nombre] = f;
    }
    if (f->cuerpo) {
        if (definidas.count(f->nombre)) {
            error("redefinición de la función " + f->nombre);
        }
        definidas.insert(f->nombre);
    }
}

// Recorre las funciones en orden: una función solo puede llamarse después de su
// declaración (prototipo o definición), como en C++. La recursión funciona porque
// la función se registra antes de verificar su cuerpo.
void TypeChecker::visit(Programa* program) {
    for (FunDec* f : program->funciones) {
        if (f->nombre == "main" && (f->tipo != "int" || !f->parametros_tipos.empty())) {
            error("main debe declararse como: int main()");
        }
        registrar(f);
        if (f->cuerpo) {
            numFunciones++;
            f->accept(this);
        }
    }
    if (!definidas.count("main")) {
        error("falta la función main");
    }
    for (const string& nombre : llamadas) {
        if (!definidas.count(nombre)) {
            error("la función " + nombre + " se declaró pero no se definió");
        }
    }
}
