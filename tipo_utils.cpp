#include <climits>
#include "tipo_utils.h"

using namespace std;

// Tipo que corresponde a un nombre de tipo (de un arreglo o puntero, el de sus elementos)
Type::Kind kindDe(const string& tipo) {
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

bool mismoTipo(const Type& a, const Type& b) {
    return a.kind == b.kind && a.nombreStruct == b.nombreStruct && a.dims == b.dims
        && a.esPuntero == b.esPuntero;
}

// Solo una variable o un elemento de arreglo se pueden enlazar a un parámetro por referencia
bool esVariable(Exp* e) {
    return dynamic_cast<IdExp*>(e) || dynamic_cast<ArrayExp*>(e);
}

// Un parámetro const T& usa la variable original si es del mismo tipo;
// si no (un literal, una expresión, otro tipo), recibe un temporal.
bool enlazaDirecto(Exp* argumento, const Type& parametro) {
    return esVariable(argumento) && mismoTipo(argumento->tipoExp, parametro);
}


// Cantidad de elementos de un arreglo
long long totalElementos(const vector<int>& dims) {
    long long n = 1;
    for (int d : dims) n *= d;
    return n;
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