#ifndef TYPE_H
#define TYPE_H

#include <string>
#include <ostream>

using namespace std;

// Resultado de visitar un nodo del AST: su tipo y su valor.
// El valor se guarda en el atributo que corresponde al tipo:
//   INT  -> intValue
//   BOOL -> boolValue
//   VOID -> ninguno (sentencias)
class Type {
public:
    enum Kind { VOID, INT, BOOL };

    Kind kind;
    int intValue;
    bool boolValue;

    Type();                       // VOID
    Type(int v);                  // INT con valor v
    Type(bool b);                 // BOOL con valor b

    bool isInt() const;
    bool isBool() const;
    bool isVoid() const;
    string kindName() const;

    friend ostream& operator<<(ostream& outs, const Type& t);
};

#endif // TYPE_H
