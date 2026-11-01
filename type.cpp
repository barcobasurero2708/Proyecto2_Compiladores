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

bool Type::isInt() const  { return kind == INT; }
bool Type::isBool() const { return kind == BOOL; }
bool Type::isVoid() const { return kind == VOID; }
bool Type::isChar() const    { return kind == CHAR; }
bool Type::isFloat() const   { return kind == FLOAT; }
bool Type::isString() const  { return kind == STRING; }
bool Type::isNumeric() const { return kind == INT || kind == CHAR || kind == FLOAT; }

// Nombre del tipo, útil para mensajes de error o depuración
string Type::kindName() const {
    switch (kind) {
        case INT:  return "int";
        case BOOL: return "bool";
        case CHAR: return "char";
        case FLOAT: return "float";
        case STRING: return "string";
        default:   return "void";
    }
}

// Imprime el tipo con su valor, por ejemplo int(5) o bool(true)
ostream& operator<<(ostream& outs, const Type& t) {
    switch (t.kind) {
        case Type::INT:  return outs << "int(" << t.intValue << ")";
        case Type::BOOL: return outs << "bool(" << (t.boolValue ? "true" : "false") << ")";
        case Type::CHAR:   return outs << "char";
        case Type::FLOAT:  return outs << "float";
        case Type::STRING: return outs << "string";
        default:         return outs << "void";
    }
}
