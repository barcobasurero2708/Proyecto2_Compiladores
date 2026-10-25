#include "type.h"

// Tipo vacío, lo devuelven las sentencias
Type::Type() : kind(VOID), intValue(0), boolValue(false) {}

// Tipo entero: el valor se guarda en intValue
Type::Type(int v) : kind(INT), intValue(v), boolValue(false) {}

// Tipo booleano: el valor se guarda en boolValue
Type::Type(bool b) : kind(BOOL), intValue(0), boolValue(b) {}

bool Type::isInt() const  { return kind == INT; }
bool Type::isBool() const { return kind == BOOL; }
bool Type::isVoid() const { return kind == VOID; }

// Nombre del tipo, útil para mensajes de error o depuración
string Type::kindName() const {
    switch (kind) {
        case INT:  return "int";
        case BOOL: return "bool";
        default:   return "void";
    }
}

// Imprime el tipo con su valor, por ejemplo int(5) o bool(true)
ostream& operator<<(ostream& outs, const Type& t) {
    switch (t.kind) {
        case Type::INT:  return outs << "int(" << t.intValue << ")";
        case Type::BOOL: return outs << "bool(" << (t.boolValue ? "true" : "false") << ")";
        default:         return outs << "void";
    }
}
