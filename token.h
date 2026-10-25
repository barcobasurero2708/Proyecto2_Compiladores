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
        PRINT,
        IF,
        WHILE,
        DO,
        THEN,
        ENDIF,
        ENDWHILE,
        ENDDO,
        ELSE,
        BREAK,
        IFEXP,
        COMMA,
        TRUE,
        FALSE,
        VAR,
        FUN,
        ENDFUN,
        RETURN,
        MAX,
        MIN,
        COUNT,
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
