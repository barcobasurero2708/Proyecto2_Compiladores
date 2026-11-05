#include "type.h"

// Tipo de la clase k, sin valor concreto
Type Type::deKind(Kind k) {
    Type t;
    t.kind = k;
    return t;
}

// Tipo vacío, lo devuelven las sentencias
Type::Type() : kind(VOID), intValue(0), boolValue(false) {}

// Tipo entero: el valor se guarda en intValue
Type::Type(int v) : kind(INT), intValue(v), boolValue(false) {}

// Tipo booleano: el valor se guarda en boolValue
Type::Type(bool b) : kind(BOOL), intValue(0), boolValue(b) {}

bool Type::isInt() const     { return kind == INT; }
bool Type::isBool() const    { return kind == BOOL; }
bool Type::isVoid() const    { return kind == VOID; }
bool Type::isChar() const    { return kind == CHAR; }
bool Type::isFloat() const   { return esFlotante(kind); }
bool Type::isString() const  { return kind == STRING && dims.empty(); }
bool Type::isNumeric() const { return esAritmetico(kind) && dims.empty(); }
bool Type::isArray() const   { return !dims.empty(); }

// bool, char, int, long, float y double son tipos aritméticos en C++
bool Type::esAritmetico(Kind k) {
    return k == INT || k == BOOL || k == CHAR || k == FLOAT || k == DOUBLE || k == LONG;
}

bool Type::esFlotante(Kind k) {
    return k == FLOAT || k == DOUBLE;
}

// Promoción entera: bool y char se convierten a int antes de operar
Type::Kind Type::promover(Kind k) {
    return (k == BOOL || k == CHAR) ? INT : k;
}

// Conversiones aritméticas usuales de C++ ([expr.arith.conv])
Type::Kind Type::comun(Kind a, Kind b) {
    if (a == DOUBLE || b == DOUBLE) return DOUBLE;
    if (a == FLOAT || b == FLOAT) return FLOAT;
    a = promover(a);
    b = promover(b);
    if (a == LONG || b == LONG) return LONG;
    return INT;
}

// Nombre del tipo, útil para mensajes de error o depuración
string Type::nombre(Kind k) {
    switch (k) {
        case INT:    return "int";
        case BOOL:   return "bool";
        case CHAR:   return "char";
        case FLOAT:  return "float";
        case DOUBLE: return "double";
        case LONG:   return "long";
        case STRING: return "string";
        default:     return "void";
    }
}

// Nombre con dimensiones, por ejemplo int[2][3]
string Type::kindName() const {
    string r = nombre(kind);
    for (int d : dims) r += d > 0 ? "[" + to_string(d) + "]" : "[]";
    return r;
}

// Imprime el tipo, por ejemplo int(5) o bool(true)
ostream& operator<<(ostream& outs, const Type& t) {
    switch (t.kind) {
        case Type::INT:  return outs << "int(" << t.intValue << ")";
        case Type::BOOL: return outs << "bool(" << (t.boolValue ? "true" : "false") << ")";
        default:         return outs << t.kindName();
    }
}
