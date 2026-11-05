#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <ostream>

using namespace std;

// Unidad léxica producida por el scanner: un tipo y el texto reconocido
class Token {
public:
    enum Type {
        PLUS, 
        MINUS, 
        MUL, 
        DIV, 
        POW, 
        LPAREN, 
        RPAREN, 
        NUM, 
        ERR, 
        ID, 
        LE,
        SEMICOL, 
        ASSIGN, 
        IF, 
        WHILE, 
        DO, 
        ELSE, 
        BREAK, 
        IFEXP, 
        COMMA, 
        TRUE, 
        FALSE,
        RETURN, 
        LBRACE, 
        RBRACE, 
        SHL, 
        TINT, 
        TBOOL, 
        COUT, 
        ENDL, 
        USING, 
        NAMESPACE,
        INCLUDE,
        QUESTION,
        COLON,
        LT,
        GT,
        GE,
        EQ,
        NE,
        FLOATNUM,
        CHARLIT,
        STRLIT,
        TCHAR,
        TFLOAT,
        TSTRING,
        TDOUBLE,     // double
        TLONG,       // long
        TVOID,       // void (solo en int main(void))
        RESERVED,    // palabra reservada de C++ que el subconjunto no usa
        MOD,         // %
        AND,         // &&
        OR,          // ||
        NOT,         // !
        INC,         // ++
        DEC,         // --
        FOR,
        CONTINUE,
        LBRACKET,    // [
        RBRACKET,    // ]
        END
    };

    Type type;
    string text;

    Token(Type type);
    Token(Type type, char c);
    Token(Type type, const string& source, int first, int last);

    friend ostream& operator<<(ostream& outs, const Token& tok);
    friend ostream& operator<<(ostream& outs, const Token* tok);
};

#endif // TOKEN_H
