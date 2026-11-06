#include <iostream>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_map>
#include <cstdio>
#include <cstdint>
#include <climits>
#include <map>
#include <cstring>
#include "ast.h"
#include "parser.h"
#include "visitor.h"

using namespace std;

// Registros donde van los argumentos de una llamada (convención System V)
static const char* REG_ARGS[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

// Tipo que corresponde a un nombre de tipo (de un arreglo o puntero, el de sus elementos)
static Type::Kind kindDe(const string& tipo) {
    string nombre = tipo.substr(0, tipo.find_first_of("[*"));
    if (nombre == "bool")   return Type::BOOL;
    if (nombre == "int")    return Type::INT;
    if (nombre == "char")   return Type::CHAR;
    if (nombre == "float")  return Type::FLOAT;
    if (nombre == "double") return Type::DOUBLE;
    if (nombre == "long")   return Type::LONG;
    if (nombre == "string") return Type::STRING;
    return nombre == "void" ? Type::VOID : Type::STRUCT;
}

// Tipo completo de un nombre de tipo, incluidas las dimensiones: "int[][3]" -> int con dims {0, 3}
Type tipoDe(const string& nombre) {
    Type t = Type::deKind(kindDe(nombre));
    t.esPuntero = nombre.back() == '*';
    size_t i = nombre.find('[');
    if (t.isStruct()) t.nombreStruct = nombre.substr(0, i);
    while (i != string::npos) {
        size_t fin = nombre.find(']', i);
        string n = nombre.substr(i + 1, fin - i - 1);
        t.dims.push_back(n.empty() ? 0 : stoi(n));
        i = nombre.find('[', fin);
    }
    return t;
}

static bool mismoTipo(const Type& a, const Type& b) {
    return a.kind == b.kind && a.nombreStruct == b.nombreStruct && a.dims == b.dims
        && a.esPuntero == b.esPuntero;
}

// Tipo con el que se convierte un valor: un puntero se trata como un entero de 64 bits
static Type::Kind kindValor(Exp* e) {
    return e->tipoExp.esPuntero ? Type::LONG : e->tipoExp.kind;
}

// Solo una variable o un elemento de arreglo se pueden enlazar a un parámetro por referencia
static bool esVariable(Exp* e) {
    return dynamic_cast<IdExp*>(e) || dynamic_cast<ArrayExp*>(e);
}

// Un parámetro const T& usa la variable original si es del mismo tipo;
// si no (un literal, una expresión, otro tipo), recibe un temporal.
static bool enlazaDirecto(Exp* argumento, const Type& parametro) {
    return esVariable(argumento) && mismoTipo(argumento->tipoExp, parametro);
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
Type StructExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type CampoExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type UnaryExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type ArrayExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type IncDecExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type DireccionExp::accept(Visitor* visitor) { return visitor->visit(this); }
Type DerefExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type LongitudExp::accept(Visitor* visitor) { return visitor->visit(this); }
Type NumberExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type IdExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type IfExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type FCallExp::accept(Visitor* visitor)   { return visitor->visit(this); }

void AssignStmt::accept(Visitor* visitor)  { visitor->visit(this); }
void AssignPunteroStmt::accept(Visitor* visitor) { visitor->visit(this); }
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
void SwitchStmt::accept(Visitor* visitor)  { visitor->visit(this); }

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
    convertir(kindValor(e), Type::BOOL);
}

// Operaciones creadas por la reducción de fuerza; el operando derecho es el exponente k.
// La división de C++ redondea hacia cero: a un negativo se le suma 2^k - 1 antes de desplazar.
void GencodeVisitor::potenciaDeDos(BinaryExp* exp) {
    long long k = dynamic_cast<NumberExp*>(exp->right)->value;
    exp->left->accept(this);
    convertir(exp->left->tipoExp.kind, exp->tipoOperandos);
    if (exp->op == SHL_OP) {
        out << "shlq $" << k << ", %rax" << endl;
    }
    else {
        if (exp->op == MOD_POT2_OP) out << "movq %rax, %rdx" << endl;   // el dividendo original
        out << "movq %rax, %rcx" << endl;
        out << "sarq $63, %rcx" << endl;                                // -1 si es negativo
        out << "shrq $" << 64 - k << ", %rcx" << endl;                  // 2^k - 1 si es negativo
        out << "addq %rcx, %rax" << endl;
        out << "sarq $" << k << ", %rax" << endl;
        if (exp->op == MOD_POT2_OP) {                                   // x - (x / 2^k) * 2^k
            out << "shlq $" << k << ", %rax" << endl;
            out << "subq %rax, %rdx" << endl;
            out << "movq %rdx, %rax" << endl;
        }
    }
    if (exp->tipoOperandos == Type::INT) out << "movslq %eax, %rax" << endl;
}

// Genera el código de left op right y deja el resultado en %rax
Type GencodeVisitor::visit(BinaryExp* exp) {
    if (!exp->sobrecarga.empty()) {
        emitirLlamada(exp->sobrecarga, {exp->left, exp->right});
        return exp->tipoExp;
    }
    if (exp->op == SHL_OP || exp->op == DIV_POT2_OP || exp->op == MOD_POT2_OP) {
        potenciaDeDos(exp);
        return exp->tipoExp;
    }
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
    convertir(kindValor(exp->left), k);
    out << "pushq %rax" << endl;
    pila++;
    exp->right->accept(this);
    convertir(kindValor(exp->right), k);
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
    return exp->tipoExp;
}

// Un agregado cabe en un slot: campo 0 abajo, campo 1 arriba.
Type GencodeVisitor::visit(StructExp* exp) {
    out << "xorl %eax, %eax" << endl;
    if (!exp->valores.empty()) {
        exp->valores[0]->accept(this);
        out << "movl %eax, %eax" << endl;  // limpia la mitad alta
    }
    if (exp->valores.size() == 2) {
        out << "pushq %rax" << endl;
        pila++;
        exp->valores[1]->accept(this);
        out << "shlq $32, %rax" << endl;
        out << "popq %rcx" << endl;
        pila--;
        out << "orq %rcx, %rax" << endl;
    }
    return exp->tipoExp;
}

Type GencodeVisitor::visit(CampoExp* exp) {
    exp->objeto->accept(this);
    if (exp->indice == 0) out << "movslq %eax, %rax" << endl;
    else out << "sarq $32, %rax" << endl;
    return exp->tipoExp;
}

// -e: se promueve el operando y se cambia el signo.  !e: e convertida a bool, invertida.
Type GencodeVisitor::visit(UnaryExp* exp) {
    if (exp->op == NOT_OP) {
        condicion(exp->e);
        out << "xorq $1, %rax" << endl;
        return Type::deKind(Type::BOOL);
    }
    exp->e->accept(this);
    convertir(exp->e->tipoExp.kind, exp->tipoExp.kind);
    switch (exp->tipoExp.kind) {
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
    return exp->tipoExp;
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
    return exp->tipoExp;
}

// Carga el valor de la variable desde la pila en %rax.
// Una referencia guarda una dirección: el valor se lee de la variable original.
Type GencodeVisitor::visit(IdExp* exp) {
    out << "movq " << posicion.lookup(exp->value) * -TAM_VARIABLE << "(%rbp), %rax" << endl;
    if (tipos.lookup(exp->value).esReferencia) out << "movq (%rax), %rax" << endl;
    return exp->tipoExp;
}

// Deja en %rax la dirección de una variable (sin índices) o de un elemento / fila de un arreglo.
// La variable de un arreglo guarda un puntero a su primer elemento; el elemento [i][j]
// está en puntero + i * paso(0) + j * paso(1).
void GencodeVisitor::direccion(const string& nombre, const vector<Exp*>& indices) {
    int offset = posicion.lookup(nombre) * -TAM_VARIABLE;
    if (indices.empty()) {
        // La dirección de una referencia es la de la variable original, que ya guarda su slot
        out << (tipos.lookup(nombre).esReferencia ? "movq " : "leaq ")
            << offset << "(%rbp), %rax" << endl;
        return;
    }
    vector<int> dims = tipos.lookup(nombre).dims;
    out << "movq " << offset << "(%rbp), %rax" << endl;
    out << "pushq %rax" << endl;
    pila++;
    for (size_t k = 0; k < indices.size(); k++) {
        indices[k]->accept(this);
        convertir(indices[k]->tipoExp.kind, Type::LONG);
        verificarIndice(nombre, dims[k]);
        out << "imulq $" << pasoIndice(dims, k) << ", %rax" << endl;
        out << "addq %rax, (%rsp)" << endl;
    }
    out << "popq %rax" << endl;
    pila--;
}

// Verificación de límites: compara sin signo, así un índice negativo también falla.
// El tamaño de un parámetro int a[] (dimensión 0) se conoce recién al ejecutar.
void GencodeVisitor::verificarIndice(const string& nombre, int dimension) {
    if (dimension > 0) {
        out << "cmpq $" << dimension << ", %rax" << endl;
    }
    else {
        out << "cmpq " << posicion.lookup(nombre + "#longitud") * -TAM_VARIABLE << "(%rbp), %rax" << endl;
    }
    out << "jae .Lerror_rango" << endl;
}

// Verificación de puntero nulo sobre la dirección que está en %rax
void GencodeVisitor::verificarNulo() {
    out << "testq %rax, %rax" << endl;
    out << "je .Lerror_puntero_nulo" << endl;
}

// s[i]: carga el carácter; como en std::string, s[s.size()] es válido y vale '\0'
void GencodeVisitor::caracterDeCadena(ArrayExp* exp) {
    vector<Exp*> hastaLaCadena(exp->indices.begin(), exp->indices.end() - 1);
    direccion(exp->nombre, hastaLaCadena);
    out << "movq (%rax), %rax" << endl;
    out << "pushq %rax" << endl;
    pila++;
    exp->indices.back()->accept(this);
    convertir(exp->indices.back()->tipoExp.kind, Type::LONG);
    out << "pushq %rax" << endl;
    pila++;
    out << "movq 8(%rsp), %rdi" << endl;
    llamar("strlen");
    out << "popq %rcx" << endl;
    out << "popq %rdx" << endl;
    pila -= 2;
    out << "cmpq %rax, %rcx" << endl;
    out << "ja .Lerror_rango" << endl;
    out << "movsbq (%rdx,%rcx), %rax" << endl;
}

// a[i][j]: carga el elemento; con menos índices que dimensiones deja la dirección de la fila
Type GencodeVisitor::visit(ArrayExp* exp) {
    if (exp->esCaracter) {
        caracterDeCadena(exp);
        return exp->tipoExp;
    }
    direccion(exp->nombre, exp->indices);
    if (exp->tipoExp.dims.empty()) out << "movq (%rax), %rax" << endl;
    return exp->tipoExp;
}

// &x, &a[i]: deja la dirección en %rax
Type GencodeVisitor::visit(DireccionExp* exp) {
    if (IdExp* variable = dynamic_cast<IdExp*>(exp->destino)) {
        direccion(variable->value, {});
    }
    else {
        ArrayExp* elemento = dynamic_cast<ArrayExp*>(exp->destino);
        direccion(elemento->nombre, elemento->indices);
    }
    return exp->tipoExp;
}

// *p: verifica el puntero y carga el valor
Type GencodeVisitor::visit(DerefExp* exp) {
    exp->puntero->accept(this);
    verificarNulo();
    out << "movq (%rax), %rax" << endl;
    return exp->tipoExp;
}

// s.size(): strlen de la cadena
Type GencodeVisitor::visit(LongitudExp* exp) {
    exp->cadena->accept(this);
    out << "movq %rax, %rdi" << endl;
    llamar("strlen");
    return exp->tipoExp;
}

// ++x / x++ / --x / x--: suma o resta 1 en la variable o elemento.
// El prefijo devuelve el valor nuevo y el sufijo el anterior.
Type GencodeVisitor::visit(IncDecExp* exp) {
    Type::Kind k = exp->tipoExp.kind;
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
    convertir(exp->thenExp->tipoExp.kind, exp->tipoExp.kind);
    out << "jmp .Lifexp_end_" << id << endl;
    out << ".Lifexp_else_" << id << ":" << endl;
    exp->elseExp->accept(this);
    convertir(exp->elseExp->tipoExp.kind, exp->tipoExp.kind);
    out << ".Lifexp_end_" << id << ":" << endl;
    return exp->tipoExp;
}

// Llamada: evalúa los argumentos, los pasa en registros y llama a la función.
// El resultado queda en %rax.
Type GencodeVisitor::visit(FCallExp* exp) {
    if (!exp->predefinida.empty()) {
        // std::max(a, b) = (a < b) ? b : a      std::min(a, b) = (b < a) ? b : a
        Type::Kind k = exp->tipoExp.kind;
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

    emitirLlamada(exp->nombre, exp->argumentos);
    return exp->tipoExp;
}

void GencodeVisitor::emitirLlamada(const string& nombre, const vector<Exp*>& argumentos) {
    FunDec* f = funciones.at(nombre);
    for (size_t i = 0; i < argumentos.size(); i++) {
        if (f->parametros_ref[i]) {
            argumentoReferencia(argumentos[i], tipoDe(f->parametros_tipos[i]));
        }
        else {
            argumentos[i]->accept(this);
            convertir(argumentos[i]->tipoExp.kind, kindDe(f->parametros_tipos[i]));
        }
        out << "pushq %rax" << endl;
        pila++;
    }
    for (size_t i = argumentos.size(); i > 0; i--) {
        out << "popq " << REG_ARGS[i - 1] << endl;
        pila--;
    }
    for (size_t i = 0; i < argumentos.size(); i++) {
        if (tipoDe(f->parametros_tipos[i]).isArray()) pasarLongitud(argumentos[i], i);
    }
    llamar(etiquetaFuncion(nombre));
}

// Un parámetro int a[] recibe solo un puntero. Su longitud se deja en .Llongitudes, justo
// antes de la llamada, y la función la copia a su pila al entrar. Si el argumento también
// es un parámetro sin tamaño conocido, se reenvía la longitud que él recibió.
void GencodeVisitor::pasarLongitud(Exp* arreglo, size_t posicionArgumento) {
    int longitud = arreglo->tipoExp.dims[0];
    string destino = ".Llongitudes+" + to_string(posicionArgumento * TAM_VARIABLE) + "(%rip)";
    if (longitud > 0) {
        out << "movq $" << longitud << ", " << destino << endl;
        return;
    }
    string nombre = dynamic_cast<IdExp*>(arreglo)->value;
    out << "movq " << posicion.lookup(nombre + "#longitud") * -TAM_VARIABLE << "(%rbp), %r10" << endl;
    out << "movq %r10, " << destino << endl;
}

// Deja en %rax la dirección que recibe un parámetro por referencia: la de la variable
// original o, en un const T&, la de un temporal con el valor ya convertido.
// El TypeChecker reservó en la pila un lugar para cada temporal.
void GencodeVisitor::argumentoReferencia(Exp* e, const Type& parametro) {
    if (IdExp* variable = dynamic_cast<IdExp*>(e); variable && enlazaDirecto(e, parametro)) {
        direccion(variable->value, {});
    }
    else if (ArrayExp* elemento = dynamic_cast<ArrayExp*>(e); elemento && enlazaDirecto(e, parametro)) {
        direccion(elemento->nombre, elemento->indices);
    }
    else {
        e->accept(this);
        convertir(e->tipoExp.kind, parametro.kind);
        int offset = contador++ * -TAM_VARIABLE;
        out << "movq %rax, " << offset << "(%rbp)" << endl;
        out << "leaq " << offset << "(%rbp), %rax" << endl;
    }
}

// Evalúa la expresión y la guarda en la variable o en el elemento del arreglo
void GencodeVisitor::visit(AssignStmt* stm) {
    Type destino = tipos.lookup(stm->id);            // tipo con el que se declaró la variable
    int slot = posicion.lookup(stm->id) * -TAM_VARIABLE;
    stm->e->accept(this);
    if (!stm->campo.empty()) {
        convertir(stm->e->tipoExp.kind, Type::INT);
        if (destino.esReferencia) {
            out << "movq " << slot << "(%rbp), %rcx" << endl;
            out << "movl %eax, " << 4 * stm->indiceCampo << "(%rcx)" << endl;
        }
        else {
            out << "movl %eax, " << slot + 4 * stm->indiceCampo << "(%rbp)" << endl;
        }
        return;
    }
    convertir(stm->e->tipoExp.kind, destino.kind);
    if (stm->indices.empty()) {
        if (destino.esReferencia) {
            // La dirección se lee al final: el valor ya está calculado y convertido
            out << "movq " << slot << "(%rbp), %rcx" << endl;
            out << "movq %rax, (%rcx)" << endl;
        }
        else {
            out << "movq %rax, " << slot << "(%rbp)" << endl;
        }
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

// *p = e: el valor se evalúa antes que la dirección, como en a[i] = e
void GencodeVisitor::visit(AssignPunteroStmt* stm) {
    stm->e->accept(this);
    convertir(stm->e->tipoExp.kind, stm->puntero->tipoExp.kind);
    out << "pushq %rax" << endl;
    pila++;
    stm->puntero->accept(this);
    verificarNulo();
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
        Type::Kind k = e->tipoExp.kind;
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

// Genera el código del switch: compara el valor con cada case y salta a la etiqueta que
// coincide (o a default, o al final). Las sentencias van seguidas, por eso sin break la
// ejecución continúa en la etiqueta siguiente. break usa finDeBucle; continue no cambia
// y sigue saltando al bucle que contiene al switch.
void GencodeVisitor::visit(SwitchStmt* stm) {
    int id = etiquetas++;
    string fin = ".Lendswitch_" + to_string(id);
    auto rotulo = [id](size_t k) { return ".Lcase_" + to_string(id) + "_" + to_string(k); };

    stm->condicion->accept(this);                  // los enteros ya están extendidos a 64 bits
    string sinCoincidencia = fin;
    for (size_t k = 0; k < stm->etiquetas.size(); k++) {
        if (!stm->etiquetas[k].expresion) {
            sinCoincidencia = rotulo(k);           // default se elige solo si ningún case coincide
            continue;
        }
        out << "movabsq $" << stm->etiquetas[k].valor << ", %rcx" << endl;
        out << "cmpq %rcx, %rax" << endl;
        out << "je " << rotulo(k) << endl;
    }
    out << "jmp " << sinCoincidencia << endl;

    finDeBucle.push_back(fin);
    posicion.add_level(); tipos.add_level();
    size_t k = 0;
    for (size_t i = 0; i < stm->sentencias.size(); i++) {
        while (k < stm->etiquetas.size() && stm->etiquetas[k].posicion == i) {
            out << rotulo(k++) << ":" << endl;
        }
        stm->sentencias[i]->accept(this);
    }
    posicion.remove_level(); tipos.remove_level();
    finDeBucle.pop_back();
    out << fin << ":" << endl;
}

// Genera el código del break: salta a la salida del bucle o switch más interno
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
        convertir(stm->exp->tipoExp.kind, kindDe(funciones[funcionActual]->tipo));   // al tipo de la función
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
                convertir(e->tipoExp.kind, destino.kind);
                out << "movq %rax, " << (primero - k) * -TAM_VARIABLE << "(%rbp)" << endl;
            }
            continue;
        }
        posicion.add_var(v, contador++);
        tipos.add_var(v, destino);
        if (dec->inicializadores[i]) {
            dec->inicializadores[i]->accept(this);
            convertir(dec->inicializadores[i]->tipoExp.kind, destino.kind);
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
            Type tipo = tipoDe(fun->parametros_tipos[i]);
            tipo.esReferencia = fun->parametros_ref[i];
            posicion.add_var(p, pos);
            tipos.add_var(p, tipo);
        }
        out << "movq " << REG_ARGS[i] << ", " << pos * -TAM_VARIABLE << "(%rbp)" << endl;
    }
    // Longitud de cada parámetro arreglo, para verificar sus índices
    for (size_t i = 0; i < fun->parametros_id.size(); i++) {
        if (!tipoDe(fun->parametros_tipos[i]).isArray()) continue;
        int pos = contador++;
        posicion.add_var(fun->parametros_id[i] + "#longitud", pos);
        out << "movq .Llongitudes+" << i * TAM_VARIABLE << "(%rip), %r10" << endl;
        out << "movq %r10, " << pos * -TAM_VARIABLE << "(%rbp)" << endl;
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
    out << ".Lmensaje_rango:.string \"Error de ejecucion: indice fuera de rango.\\n\"" << endl;
    out << ".Lmensaje_puntero_nulo:.string \"Error de ejecucion: puntero nulo.\\n\"" << endl;
    out << ".Llongitudes:.zero " << 6 * TAM_VARIABLE << endl;
    out << ".text" << endl;
    out << ".globl main" << endl;
    for (FunDec* f : program->funciones) f->accept(this);
    // Se llega por salto desde cualquier expresión. No retorna y realinea la
    // pila, incluso si había operandos pendientes o llamadas recursivas.
    for (const char* falla : {"division_cero", "rango", "puntero_nulo"}) {
        out << ".Lerror_" << falla << ":" << endl;
        out << "andq $-16, %rsp" << endl;
        out << "leaq .Lmensaje_" << falla << "(%rip), %rdi" << endl;
        out << "movq stderr(%rip), %rsi" << endl;
        out << "call fputs@PLT" << endl;
        out << "movl $1, %edi" << endl;
        out << "call exit@PLT" << endl;
    }
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
static bool convertible(const Type& destino, const Type& origen) {
    return mismoTipo(destino, origen) || (destino.isNumeric() && origen.isNumeric())
        || (destino.esPuntero && origen.isNulo());
}

// Una condición acepta números y punteros (un puntero es falso si es nullptr)
static bool convertibleABool(const Type& t) {
    return t.isNumeric() || t.esPuntero;
}

// Anota el tipo completo que utilizará el generador de ensamblador.
static Type anotar(Exp* e, const Type& tipo) {
    e->tipoExp = tipo;
    return tipo;
}

static Type anotar(Exp* e, Type::Kind k) {
    return anotar(e, Type::deKind(k));
}

// Los enteros (bool, char, int, long) sirven como índice y para %
static bool esEntero(const Type& t) {
    return t.isNumeric() && !t.isFloat();
}

// Calcula el valor de una expresión constante entera (una etiqueta case, un índice).
// Devuelve false si no es constante o si desborda, que en C++ también es un error.
bool constanteEntera(Exp* e, long long& valor) {
    if (IdExp* id = dynamic_cast<IdExp*>(e)) {
        valor = id->tipoExp.valorConst;        // una variable const de valor conocido
        return id->tipoExp.tieneValor;
    }
    if (NumberExp* n = dynamic_cast<NumberExp*>(e)) {
        if (Type::esFlotante(n->tipo) || n->tipo == Type::STRING) return false;
        valor = n->value;
        return true;
    }
    if (UnaryExp* u = dynamic_cast<UnaryExp*>(e)) {
        long long a;
        if (!constanteEntera(u->e, a)) return false;
        if (u->op == NOT_OP) {
            valor = !a;
            return true;
        }
        if (a == LLONG_MIN || (u->tipoExp.isInt() && -a > INT_MAX)) return false;
        valor = -a;
        return true;
    }
    if (IfExp* c = dynamic_cast<IfExp*>(e)) {
        long long a;
        if (!constanteEntera(c->condicion, a)) return false;
        return constanteEntera(a ? c->thenExp : c->elseExp, valor);
    }
    BinaryExp* b = dynamic_cast<BinaryExp*>(e);
    long long a, c, r = 0;
    if (!b || !b->sobrecarga.empty() || !constanteEntera(b->left, a)) return false;
    if ((b->op == AND_OP && !a) || (b->op == OR_OP && a)) {    // cortocircuito
        valor = a != 0;
        return true;
    }
    if (!constanteEntera(b->right, c)) return false;
    bool desborda = false;
    switch (b->op) {
        case PLUS_OP:  desborda = __builtin_add_overflow(a, c, &r); break;
        case MINUS_OP: desborda = __builtin_sub_overflow(a, c, &r); break;
        case MUL_OP:   desborda = __builtin_mul_overflow(a, c, &r); break;
        case DIV_OP: case MOD_OP:
            if (c == 0 || (a == LLONG_MIN && c == -1)) return false;
            r = b->op == DIV_OP ? a / c : a % c;
            break;
        case LT_OP:  r = a < c;  break;
        case LE_OP:  r = a <= c; break;
        case GT_OP:  r = a > c;  break;
        case GE_OP:  r = a >= c; break;
        case EQ_OP:  r = a == c; break;
        case NE_OP:  r = a != c; break;
        case AND_OP: r = a && c; break;
        case OR_OP:  r = a || c; break;
        default:     return false;
    }
    if (desborda || (b->tipoOperandos == Type::INT && (r < INT_MIN || r > INT_MAX))) return false;
    valor = r;
    return true;
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
    if (!convertibleABool(c)) {
        error("la condición del " + donde + " no se puede convertir a bool (es " + c.kindName() + ")");
    }
}

// ---------- Expresiones ----------

int TypeChecker::indiceCampo(const Type& tipo, const string& campo) {
    if (!tipo.isStruct() || tipo.isArray()) {
        error("no se puede acceder al campo '" + campo + "' de " + tipo.kindName());
    }
    const auto& campos = estructuras.at(tipo.nombreStruct);
    auto it = find(campos.begin(), campos.end(), campo);
    if (it == campos.end()) error(tipo.kindName() + " no tiene el campo '" + campo + "'");
    return static_cast<int>(it - campos.begin());
}

Type TypeChecker::visit(CampoExp* exp) {
    exp->indice = indiceCampo(exp->objeto->accept(this), exp->campo);
    return anotar(exp, Type::INT);
}

Type TypeChecker::visit(StructExp* exp) {
    const auto& campos = estructuras.at(exp->nombre);
    if (exp->valores.size() > campos.size()) {
        error("demasiados inicializadores para " + exp->nombre);
    }
    for (Exp* valor : exp->valores) {
        Type t = valor->accept(this);
        if (!t.isNumeric() || Type::promover(t.kind) != Type::INT) {
            error("el inicializador de un campo int de " + exp->nombre + " no admite " + t.kindName());
        }
    }
    return anotar(exp, tipoDe(exp->nombre));
}

// Orden de conversiones estándar admitidas: exacta, promoción, conversión.
static int rangoConversion(const Type& parametro, const Type& argumento) {
    if (mismoTipo(parametro, argumento)) return 0;
    if (!parametro.isNumeric() || !argumento.isNumeric()) return -1;
    if (parametro.kind == Type::promover(argumento.kind)
        || (parametro.kind == Type::DOUBLE && argumento.kind == Type::FLOAT)) return 1;
    return 2;
}

Type TypeChecker::resolverSobrecarga(BinaryExp* exp, const Type& izq, const Type& der) {
    struct Candidato {
        FunDec* funcion;
        array<int, 2> rangos;
    };
    vector<Candidato> candidatos;
    string operador = Exp::binopToChar(exp->op);
    // T& solo admite una variable no const del mismo tipo; const T& acepta como un parámetro por valor
    auto rango = [](FunDec* funcion, size_t i, Exp* argumento, const Type& tipo) {
        Type parametro = tipoDe(funcion->parametros_tipos[i]);
        if (!funcion->parametros_ref[i] || funcion->parametros_const[i]) {
            return rangoConversion(parametro, tipo);
        }
        return esVariable(argumento) && mismoTipo(parametro, tipo) && !tipo.esConst ? 0 : -1;
    };
    for (const auto& entrada : funciones) {
        FunDec* funcion = entrada.second;
        if (funcion->operador != operador) continue;
        int rangoIzq = rango(funcion, 0, exp->left, izq);
        int rangoDer = rango(funcion, 1, exp->right, der);
        if (rangoIzq >= 0 && rangoDer >= 0) {
            candidatos.push_back({funcion, {rangoIzq, rangoDer}});
        }
    }

    string operandos = izq.kindName() + " y " + der.kindName();
    if (candidatos.empty()) error("no hay sobrecarga viable de operator" + operador + " para " + operandos);
    for (const Candidato& candidato : candidatos) {
        bool mejor = true;
        for (const Candidato& otro : candidatos) {
            if (&candidato == &otro) continue;
            if (candidato.rangos[0] > otro.rangos[0] || candidato.rangos[1] > otro.rangos[1]
                || candidato.rangos == otro.rangos) {
                mejor = false;
                break;
            }
        }
        if (mejor) {
            exp->sobrecarga = candidato.funcion->nombre;
            llamadas.insert(exp->sobrecarga);
            reservarTemporales(candidato.funcion, {exp->left, exp->right});
            return anotar(exp, tipoDe(candidato.funcion->tipo));
        }
    }
    error("sobrecarga ambigua de operator" + operador + " para " + operandos);
    return Type();
}

// Aritméticos: conversiones aritméticas usuales. Comparaciones y lógicos: devuelven bool.
Type TypeChecker::visit(BinaryExp* exp) {
    Type l = exp->left->accept(this);
    Type r = exp->right->accept(this);
    if (l.isStruct() || r.isStruct()) return resolverSobrecarga(exp, l, r);
    string op = Exp::binopToChar(exp->op);
    string operandos = l.kindName() + " " + op + " " + r.kindName();
    bool comparacion = exp->op != PLUS_OP && exp->op != MINUS_OP && exp->op != MUL_OP
                    && exp->op != DIV_OP && exp->op != MOD_OP;

    if (exp->op == AND_OP || exp->op == OR_OP) {
        if (!convertibleABool(l) || !convertibleABool(r)) {
            error("el operador '" + op + "' necesita operandos convertibles a bool, pero recibió " + operandos);
        }
        return anotar(exp, Type::BOOL);
    }
    // Los punteros solo se comparan por igualdad, con otro del mismo tipo o con nullptr
    if (l.esPuntero || r.esPuntero) {
        bool igualdad = exp->op == EQ_OP || exp->op == NE_OP;
        if (!igualdad || !(mismoTipo(l, r) || convertible(l, r) || convertible(r, l))) {
            error("el operador '" + op + "' no se puede aplicar a " + operandos);
        }
        exp->tipoOperandos = Type::LONG;
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
    if (exp->op == NOT_OP && t.esPuntero) return anotar(exp, Type::BOOL);
    if (!t.isNumeric()) {
        error("el operador '" + op + "' no se puede aplicar a " + t.kindName());
    }
    if (exp->op == NOT_OP) return anotar(exp, Type::BOOL);
    return anotar(exp, Type::promover(t.kind));
}

// Verifica nombre[i]...[j] (o solo nombre) como destino de una asignación o de ++ / --
// y devuelve el tipo del elemento
Type TypeChecker::destino(const string& nombre, const vector<Exp*>& indices, const string& accion,
                          vector<int>& dimensiones) {
    if (!tipos.check(nombre)) {
        error("variable no declarada: " + nombre);
    }
    Type t = tipos.lookup(nombre);
    dimensiones = t.dims;
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
    if (t.esConst) {
        error("no se puede " + accion + " la constante " + nombre);
    }
    t.dims.clear();
    return t;
}

// a[i][j]: a es un arreglo, cada índice es entero. Con menos índices que dimensiones
// el resultado es una fila (un arreglo)
Type TypeChecker::visit(ArrayExp* exp) {
    if (!tipos.check(exp->nombre)) {
        error("variable no declarada: " + exp->nombre);
    }
    Type t = tipos.lookup(exp->nombre);
    exp->dimensiones = t.dims;
    if (t.kind == Type::STRING && exp->indices.size() == t.dims.size() + 1) {   // s[i], n[0][i]
        exp->esCaracter = true;
        for (Exp* i : exp->indices) {
            Type ti = i->accept(this);
            if (!esEntero(ti)) error("el índice de " + exp->nombre + " debe ser entero, pero es " + ti.kindName());
        }
        return anotar(exp, Type::CHAR);
    }
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
    t.dims.erase(t.dims.begin(), t.dims.begin() + exp->indices.size());
    return anotar(exp, t);
}

// ++x / x-- : x es una variable o elemento numérico. En C++17 no se permite sobre bool.
Type TypeChecker::visit(IncDecExp* exp) {
    string op = exp->incremento ? "++" : "--";
    Type t = destino(exp->nombre, exp->indices, "aplicar '" + op + "' a", exp->dimensiones);
    if (!t.isNumeric() || t.isBool()) {
        error("el operador '" + op + "' no se puede aplicar a " + t.kindName());
    }
    return anotar(exp, t.kind);
}

// &x: x es una variable numérica no const. El resultado es un puntero a su tipo.
Type TypeChecker::visit(DireccionExp* exp) {
    Type t = exp->destino->accept(this);
    if (!t.isNumeric()) error("'&' necesita una variable numérica, pero recibió " + t.kindName());
    if (t.esConst) error("no se puede tomar la dirección de una constante");
    if (ArrayExp* elemento = dynamic_cast<ArrayExp*>(exp->destino); elemento && elemento->esCaracter) {
        error("no se puede tomar la dirección de un carácter de un string");
    }
    Type puntero = Type::deKind(t.kind);
    puntero.esPuntero = true;
    return anotar(exp, puntero);
}

// *p: el resultado es el tipo apuntado
Type TypeChecker::visit(DerefExp* exp) {
    Type t = exp->puntero->accept(this);
    if (!t.esPuntero || t.isNulo()) error("'*' necesita un puntero, pero recibió " + t.kindName());
    return anotar(exp, t.kind);
}

// s.size() devuelve un entero; aquí se trata como long
Type TypeChecker::visit(LongitudExp* exp) {
    Type t = exp->cadena->accept(this);
    if (!t.isString()) error("size() y length() necesitan un string, pero recibieron " + t.kindName());
    return anotar(exp, Type::LONG);
}

// Tipo del literal
Type TypeChecker::visit(NumberExp* exp) {
    if (exp->tipo == Type::VOID) return anotar(exp, Type::nulo());
    return anotar(exp, exp->esBool ? Type::BOOL : exp->tipo);
}

// Una variable tiene el tipo con el que fue declarada
Type TypeChecker::visit(IdExp* exp) {
    if (!tipos.check(exp->value)) {
        error("variable no declarada: " + exp->value);
    }
    Type t = tipos.lookup(exp->value);
    return anotar(exp, t);
}

// c ? e1 : e2: c convertible a bool; si las ramas son aritméticas de distinto tipo
// se aplica la conversión aritmética usual
Type TypeChecker::visit(IfExp* exp) {
    Type c = exp->condicion->accept(this);
    if (!convertibleABool(c)) {
        error("la condición de ?: no se puede convertir a bool (es " + c.kindName() + ")");
    }
    Type t = exp->thenExp->accept(this);
    Type e = exp->elseExp->accept(this);
    if (t.isArray() || e.isArray()) {
        error("las ramas de ?: no pueden ser arreglos");
    }
    if (mismoTipo(t, e)) return anotar(exp, t);
    if (t.isNumeric() && e.isNumeric()) return anotar(exp, Type::comun(t.kind, e.kind));
    error("las ramas de ?: tienen tipos incompatibles: " + t.kindName() + " y " + e.kindName());
    return Type();
}

// Reserva un lugar en la pila para cada const T& que recibirá un temporal
void TypeChecker::reservarTemporales(FunDec* f, const vector<Exp*>& argumentos) {
    for (size_t i = 0; i < argumentos.size(); i++) {
        if (f->parametros_ref[i] && !enlazaDirecto(argumentos[i], tipoDe(f->parametros_tipos[i]))) {
            numVariables[funcionActual->nombre]++;
        }
    }
}

// Comprueba que cada argumento (ya visitado: tiposArgs) sea convertible al tipo de su
// parámetro en f, y anota la llamada con el tipo de retorno de f.
Type TypeChecker::validarLlamada(FCallExp* exp, FunDec* f, const vector<Type>& tiposArgs) {
    llamadas.insert(f->nombre);
    if (f->tipo == "auto") {
        error("no se puede llamar a " + f->nombre + " antes de deducir su tipo de retorno");
    }
    if (tiposArgs.size() != f->parametros_tipos.size()) {
        error("la función " + f->nombre + " espera " + to_string(f->parametros_tipos.size())
              + " argumento(s), pero recibió " + to_string(tiposArgs.size()));
    }
    for (size_t i = 0; i < tiposArgs.size(); i++) {
        const Type& t = tiposArgs[i];
        Type p = tipoDe(f->parametros_tipos[i]);
        if (f->parametros_ref[i] && f->parametros_const[i]) {
            if (t.isArray() || !convertible(p, t)) {
                error("el argumento " + to_string(i + 1) + " de " + f->nombre + " debe ser "
                      + f->parametros_tipos[i] + ", pero es " + t.kindName());
            }
            continue;
        }
        if (f->parametros_ref[i]) {
            Exp* argumento = exp->argumentos[i];
            string posicion = "el argumento " + to_string(i + 1) + " de " + f->nombre;
            if (dynamic_cast<CampoExp*>(argumento)) {
                error(posicion + ": un campo de struct por referencia no está soportado");
            }
            if (!esVariable(argumento)) {
                error(posicion + " es por referencia: debe ser una variable o un elemento de arreglo");
            }
            if (!mismoTipo(p, t)) {
                error(posicion + " es por referencia: debe ser exactamente "
                      + f->parametros_tipos[i] + ", pero es " + t.kindName());
            }
            if (t.esConst) {
                error(posicion + " es por referencia y no puede recibir una constante");
            }
            continue;
        }
        bool ok;
        if (p.isArray() || t.isArray()) {
            ok = p.isArray() && t.isArray() && p.kind == t.kind && p.dims.size() == t.dims.size()
                 && equal(p.dims.begin() + 1, p.dims.end(), t.dims.begin() + 1)
                 && (!t.esConst || f->parametros_const[i]);
        }
        else {
            ok = convertible(p, t);
        }
        if (!ok) {
            error("el argumento " + to_string(i + 1) + " de " + f->nombre + " debe ser "
                  + f->parametros_tipos[i] + ", pero es " + t.kindName());
        }
    }
    reservarTemporales(f, exp->argumentos);
    return anotar(exp, tipoDe(f->tipo));
}

// Llamada a una plantilla: deduce los tipos de T a partir de los argumentos (o de f<int>(...)),
// crea la instancia si todavía no existe y convierte la llamada en una llamada ordinaria
// a esa instancia (exp->nombre pasa a ser el nombre de la instancia, p. ej. "maximo.int").
Type TypeChecker::llamarPlantilla(FCallExp* exp) {
    const Plantilla& pl = programa->plantillas.at(exp->nombre);
    if (exp->argumentos.size() != pl.paramTipos.size()) {
        error("la plantilla " + pl.nombre + " espera " + to_string(pl.paramTipos.size())
              + " argumento(s), pero recibió " + to_string(exp->argumentos.size()));
    }
    if (exp->targs.size() > pl.tparams.size()) {
        error("demasiados argumentos de plantilla para " + pl.nombre);
    }
    for (const string& t : exp->targs) {
        if (t.back() == '*') error("una plantilla no admite punteros (llamada a " + pl.nombre + ")");
    }
    vector<Type> tiposArgs;
    for (Exp* a : exp->argumentos) tiposArgs.push_back(a->accept(this));

    // 1) Tipos dados explícitamente: se respetan (los argumentos se convertirán a ellos)
    map<string, string> enlace;
    set<string> explicitos;
    for (size_t k = 0; k < exp->targs.size(); k++) {
        enlace[pl.tparams[k]] = exp->targs[k];
        explicitos.insert(pl.tparams[k]);
    }
    // 2) Deducción: cada parámetro cuyo tipo es exactamente T fija T con el tipo de su argumento.
    //    Como en C++, todos los argumentos que fijan la misma T deben tener el mismo tipo.
    for (size_t i = 0; i < tiposArgs.size(); i++) {
        const string& pt = pl.paramTipos[i];
        if (find(pl.tparams.begin(), pl.tparams.end(), pt) == pl.tparams.end()) continue;
        if (explicitos.count(pt)) continue;
        const Type& t = tiposArgs[i];
        if (t.isArray()) error("no se puede deducir " + pt + " a partir de un arreglo (llamada a " + pl.nombre + ")");
        if (t.isVoid())  error("no se puede deducir " + pt + " a partir de una expresión void (llamada a " + pl.nombre + ")");
        if (t.esPuntero) error("una plantilla no admite punteros (llamada a " + pl.nombre + ")");
        string nombreTipo = t.kindName();
        auto it = enlace.find(pt);
        if (it == enlace.end()) enlace[pt] = nombreTipo;
        else if (it->second != nombreTipo) {
            error("conflicto al deducir " + pt + " en " + pl.nombre + ": " + it->second + " y " + nombreTipo);
        }
    }
    vector<string> tipos;
    string instancia = pl.nombre;
    for (const string& t : pl.tparams) {
        auto it = enlace.find(t);
        if (it == enlace.end()) {
            error("no se puede deducir el parámetro de plantilla " + t + " en la llamada a " + pl.nombre);
        }
        tipos.push_back(it->second);
        instancia += "." + it->second;       // '.' no puede aparecer en un identificador: sin choques
    }
    if (!funciones.count(instancia)) instanciar(pl, tipos, instancia);
    exp->nombre = instancia;
    return validarLlamada(exp, funciones[instancia], tiposArgs);
}

// Crea la instancia de la plantilla: segundo parseo con T := tipo, registro y verificación.
// Se registra ANTES de verificar el cuerpo, así una instancia recursiva se encuentra a sí misma.
void TypeChecker::instanciar(const Plantilla& pl, const vector<string>& argsTipo, const string& nombre) {
    string lista;
    for (const string& t : argsTipo) lista += (lista.empty() ? "" : ", ") + t;
    FunDec* inst = nullptr;
    try {
        inst = Parser::instanciar(pl, argsTipo, estructuras);
    } catch (const exception& e) {
        error(string(e.what()) + " (al instanciar " + pl.nombre + "<" + lista + ">)");
    }
    inst->nombre = nombre;
    registrar(inst);
    instancias.push_back(inst);
    numFunciones++;

    // La instancia se verifica con un entorno limpio: no ve las variables de quien la llama
    FunDec* actual = funcionActual;
    int bucleActual = bucles;
    int switchActual = switches;
    Environment<Type> entorno = tipos;
    tipos.clear();
    bucles = 0;
    switches = 0;
    try {
        inst->accept(this);
    } catch (const exception& e) {
        error(string(e.what()) + " (al instanciar " + pl.nombre + "<" + lista + ">)");
    }
    funcionActual = actual;
    bucles = bucleActual;
    switches = switchActual;
    tipos = entorno;
}

// f(a1, ..., an): la función ya está declarada (como en C++, antes de usarse),
// recibe n argumentos y cada uno es convertible al tipo de su parámetro.
// max / min sin declarar se resuelven a std::max / std::min.
Type TypeChecker::visit(FCallExp* exp) {
    const string& nombre = exp->nombre;
    bool calificado = nombre.rfind("std::", 0) == 0;

    if (!calificado && programa->plantillas.count(nombre)) {
        return llamarPlantilla(exp);
    }
    if (!calificado && funciones.count(nombre)) {
        FunDec* f = funciones[nombre];
        if (exp->argumentos.size() != f->parametros_tipos.size()) {
            error("la función " + f->nombre + " espera " + to_string(f->parametros_tipos.size())
                  + " argumento(s), pero recibió " + to_string(exp->argumentos.size()));
        }
        vector<Type> tiposArgs;
        for (Exp* a : exp->argumentos) tiposArgs.push_back(a->accept(this));
        return validarLlamada(exp, f, tiposArgs);
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
        if (a.isStruct() || b.isStruct()) {
            error(nombre + " no admite struct en este subconjunto");
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
    Type declarado = destino(stm->id, stm->indices, "asignar a", stm->dimensiones);
    if (!stm->campo.empty()) {
        stm->indiceCampo = indiceCampo(declarado, stm->campo);
        declarado = Type::deKind(Type::INT);
    }
    if (t.isArray()) {
        error("no se puede asignar un arreglo");
    }
    bool ok = convertible(declarado, t) || (declarado.isString() && t.isNumeric());
    if (!ok) {
        error("no se puede asignar " + t.kindName() + " a la variable "
              + declarado.kindName() + " " + stm->id);
    }
}

// *p = e: p es un puntero y e se puede convertir al tipo apuntado
void TypeChecker::visit(AssignPunteroStmt* stm) {
    Type t = stm->e->accept(this);
    Type puntero = stm->puntero->accept(this);
    if (!puntero.esPuntero || puntero.isNulo()) {
        error("'*' necesita un puntero, pero recibió " + puntero.kindName());
    }
    if (!convertible(Type::deKind(puntero.kind), t)) {
        error("no se puede asignar " + t.kindName() + " a través de un " + puntero.kindName());
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
        if (t.isStruct()) error("cout necesita un campo de " + t.kindName());
        if (t.esPuntero) error("cout no puede imprimir un puntero");
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

// switch: condición entera y etiquetas constantes, sin repetir. El bloque es un solo
// ámbito, y como en C++ una etiqueta no puede saltar sobre una inicialización.
void TypeChecker::visit(SwitchStmt* stm) {
    Type c = stm->condicion->accept(this);
    if (!esEntero(c)) {
        error("la condición del switch debe ser entera, pero es " + c.kindName());
    }
    bool esInt = Type::promover(c.kind) == Type::INT;   // bool y char se promueven a int
    set<long long> vistos;
    bool hayDefault = false;
    for (SwitchStmt::Etiqueta& etiqueta : stm->etiquetas) {
        if (!etiqueta.expresion) {
            if (hayDefault) error("el switch tiene más de un default");
            hayDefault = true;
            continue;
        }
        Type t = etiqueta.expresion->accept(this);
        if (!esEntero(t) || !constanteEntera(etiqueta.expresion, etiqueta.valor)) {
            error("la etiqueta case debe ser una constante entera");
        }
        if (esInt && (etiqueta.valor < INT_MIN || etiqueta.valor > INT_MAX)) {
            error("la etiqueta case " + to_string(etiqueta.valor) + " no cabe en int");
        }
        if (!vistos.insert(etiqueta.valor).second) {
            error("etiqueta case repetida: " + to_string(etiqueta.valor));
        }
    }

    size_t ultima = stm->etiquetas.empty() ? 0 : stm->etiquetas.back().posicion;
    switches++;
    tipos.add_level();
    for (size_t i = 0; i < stm->sentencias.size(); i++) {
        stm->sentencias[i]->accept(this);
        VarDec* dec = dynamic_cast<VarDec*>(stm->sentencias[i]);
        if (!dec || i >= ultima) continue;              // ninguna etiqueta salta sobre ella
        for (size_t j = 0; j < dec->variables.size(); j++) {
            // Un arreglo necesita ejecutar su declaración para fijar el puntero al primer elemento
            if (dec->inicializadores[j] || dec->tipo == "string" || !dec->dimensiones[j].empty()) {
                error("una etiqueta del switch salta sobre la declaración de " + dec->variables[j]
                      + "; encierre el caso entre { }");
            }
        }
    }
    tipos.remove_level();
    switches--;
}

// break: solo dentro de un bucle o switch
void TypeChecker::visit(BreakStmt* stm) {
    if (bucles == 0 && switches == 0) {
        error("'break' fuera de un bucle o switch");
    }
}

// continue: solo dentro de un bucle
void TypeChecker::visit(ContinueStmt* stm) {
    if (bucles == 0) {
        error("'continue' fuera de un bucle");
    }
}

// El primer return de una función auto fija su tipo; los siguientes se comparan con ese tipo
void TypeChecker::deducirRetorno(ReturnStmt* stm) {
    Type t = stm->exp ? stm->exp->accept(this) : Type();
    if (t.isArray() || t.isNulo()) {
        error("no se puede deducir el tipo de retorno de " + funcionActual->nombre);
    }
    funcionActual->tipo = t.kindName();
}

// return [e]: en una función void no hay valor (o es una llamada void);
// en las demás, e convertible al tipo de la función
void TypeChecker::visit(ReturnStmt* stm) {
    if (funcionActual->tipo == "auto") deducirRetorno(stm);
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
    bool distinto = funcionActual->esAuto && t.kindName() != funcionActual->tipo;
    if (distinto || t.isArray() || !convertible(tipoDe(funcionActual->tipo), t)) {
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

// auto x = e: x toma el tipo de e, sin const, igual que en C++
void TypeChecker::deducirAuto(VarDec* dec) {
    Type t = dec->inicializadores[0]->accept(this);
    if (t.isVoid() || t.isArray() || t.isNulo()) {
        error("no se puede deducir el tipo de " + dec->variables[0] + " a partir de " + t.kindName());
    }
    dec->tipo = t.kindName();
}

// tipo ids: sin variables repetidas en el mismo ámbito. Como en C++, la variable
// ya es visible dentro de su propio inicializador.
void TypeChecker::visit(VarDec* dec) {
    bool esAuto = dec->tipo == "auto";
    if (esAuto) deducirAuto(dec);
    Type tipo = tipoDe(dec->tipo);
    tipo.esConst = dec->esConst;
    if (tipo.isVoid()) {
        error("una variable no puede ser de tipo " + dec->tipo);
    }
    for (size_t i = 0; i < dec->variables.size(); i++) {
        const string& v = dec->variables[i];
        if (tipos.check_local(v)) {
            error("variable declarada dos veces: " + v);
        }
        if (!dec->dimensiones[i].empty()) {
            // Arreglo: n elementos más el puntero al primero
            Type arreglo = tipo;
            arreglo.dims = dec->dimensiones[i];
            tipos.add_var(v, arreglo);
            numVariables[funcionActual->nombre] += totalElementos(arreglo.dims) + 1;
            for (Exp* e : dec->listas[i]) {
                if (!e) continue;
                Type t = e->accept(this);
                if (t.isArray() || !convertible(tipo, t)) {
                    error("no se puede inicializar un elemento de " + v + " (" + dec->tipo
                          + ") con un valor " + t.kindName());
                }
            }
            continue;
        }
        tipos.add_var(v, tipo);
        numVariables[funcionActual->nombre]++;      // una variable local más
        if (dec->inicializadores[i]) {
            Type t = dec->inicializadores[i]->accept(this);
            if (esAuto && t.kindName() != dec->tipo) {
                error("auto dedujo tipos distintos: " + dec->tipo + " y " + t.kindName());
            }
            if (t.isArray() || !convertible(tipo, t)) {
                error("no se puede inicializar la variable " + dec->tipo + " " + v
                      + " con un valor " + t.kindName());
            }
            // Un const entero con valor conocido sirve en un case y lo aprovecha el plegado
            long long valor;
            if (dec->esConst && esEntero(tipo) && esEntero(t)
                && constanteEntera(dec->inicializadores[i], valor)) {
                Type constante = tipo;
                constante.tieneValor = true;
                constante.valorConst = Type::ajustarEntero(tipo.kind, valor);
                tipos.update(v, constante);
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
        if (tipoDe(fun->parametros_tipos[i]).isArray()) numVariables[fun->nombre]++;   // su longitud
        if (p.empty()) continue;
        if (tipos.check_local(p)) {
            error("parámetro repetido en " + fun->nombre + ": " + p);
        }
        Type tipo = tipoDe(fun->parametros_tipos[i]);
        tipo.esConst = fun->parametros_const[i];
        tipos.add_var(p, tipo);
    }
    fun->cuerpo->accept(this);
    if (fun->tipo == "auto") fun->tipo = "void";    // ningún return: la función auto es void
    tipos.remove_level();
    funcionActual = nullptr;
}

// const solo distingue a una firma en referencias y arreglos: f(const int) es f(int)
static vector<bool> constDeLaFirma(FunDec* f) {
    vector<bool> firma;
    for (size_t i = 0; i < f->parametros_tipos.size(); i++) {
        bool arreglo = f->parametros_tipos[i].find('[') != string::npos;
        firma.push_back(f->parametros_const[i] && (f->parametros_ref[i] || arreglo));
    }
    return firma;
}

// Registra la declaración (prototipo o definición) de una función y la compara con
// las anteriores del mismo nombre
void TypeChecker::registrar(FunDec* f) {
    if (f->nombre == "main" && f->tipo == "auto") error("main debe devolver int");
    if (!f->operador.empty()) {
        string nombre = "operator" + f->operador;
        if (f->parametros_tipos.size() != 2) {
            error(nombre + " necesita dos parámetros en este subconjunto");
        }
        if (!tipoDe(f->parametros_tipos[0]).isStruct() && !tipoDe(f->parametros_tipos[1]).isStruct()) {
            error(nombre + " necesita al menos un parámetro struct");
        }
    }
    if (f->parametros_tipos.size() > 6) {
        error("la función " + f->nombre + " tiene más de 6 parámetros");
    }
    if (funciones.count(f->nombre)) {
        FunDec* previa = funciones[f->nombre];
        if (previa->parametros_tipos != f->parametros_tipos || previa->parametros_ref != f->parametros_ref
            || constDeLaFirma(previa) != constDeLaFirma(f)) {
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
    estructuras = program->estructuras;
    programa = program;
    for (FunDec* f : program->funciones) {
        if (program->plantillas.count(f->nombre)) {
            error("'" + f->nombre + "' ya está declarada como plantilla");
        }

        registrar(f);
        if (f->cuerpo) {
            numFunciones++;
            f->accept(this);
        }
    }
    for (FunDec* inst : instancias) program->funciones.push_back(inst);
    if (!definidas.count("main")) {
        error("falta la función main");
    }
    for (const string& nombre : llamadas) {
        if (!definidas.count(nombre)) {
            error("la función " + nombre + " se declaró pero no se definió");
        }
    }
}
