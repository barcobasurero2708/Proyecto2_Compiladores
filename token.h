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
        SQRT, 
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
        MAX, 
        MIN, 
        COUNT,
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
