#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "gencode_visitor.h"
#include "type_checker_visitor.h"   
#include "tipo_utils.h"

// Registros donde van los argumentos de una llamada (convención System V)
static const char* REG_ARGS[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};


// Tipo con el que se convierte un valor: un puntero se trata como un entero de 64 bits
static Type::Kind kindValor(Exp* e) {
    return e->tipoExp.esPuntero ? Type::LONG : e->tipoExp.kind;
}

// Bytes entre dos elementos consecutivos del índice k (cada elemento ocupa 8 bytes)
static long long pasoIndice(const vector<int>& dims, size_t k) {
    long long paso = TAM_VARIABLE;
    for (size_t j = k + 1; j < dims.size(); j++) paso *= dims[j];
    return paso;
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
    // Aritmética de punteros: el entero se multiplica por el tamaño de un elemento
    bool punteroIzq = exp->left->tipoExp.esPuntero, punteroDer = exp->right->tipoExp.esPuntero;
    bool aritmeticaPuntero = (exp->op == PLUS_OP || exp->op == MINUS_OP) && (punteroIzq || punteroDer);
    exp->left->accept(this);
    convertir(kindValor(exp->left), k);
    if (aritmeticaPuntero && !punteroIzq) out << "imulq $" << TAM_VARIABLE << ", %rax" << endl;
    out << "pushq %rax" << endl;
    pila++;
    exp->right->accept(this);
    convertir(kindValor(exp->right), k);
    if (aritmeticaPuntero && !punteroDer) out << "imulq $" << TAM_VARIABLE << ", %rax" << endl;
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
    // p - q: la diferencia en bytes es múltiplo exacto de 8 = 2^3; se pasa a elementos
    if (aritmeticaPuntero && punteroIzq && punteroDer) out << "sarq $3, %rax" << endl;
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