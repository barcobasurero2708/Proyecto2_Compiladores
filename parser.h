#ifndef PARSER_H
#define PARSER_H

#include "scanner.h"
#include "ast.h"

// modificación a Parser
class Parser {
private:
    Scanner* scanner;
    Token *current, *previous;
    int loopDepth = 0;   // cantidad de bucles abiertos (para validar break)
    bool match(Token::Type ttype);
    bool check(Token::Type ttype);
    bool advance();
    bool isAtEnd();
    void consume(Token::Type ttype, const string& mensaje);
    Exp* parseCondition();

public:
    Parser(Scanner* scanner);
    Programa* parseProgram();
    Body* parseBody();
    VarDec* parseVarDec(list<Stmt*>& inicializaciones);
    FunDec* parseFunDec();
    string parseTipo();
    Stmt* parseStmt();
    list<Stmt*> parseBlock();      // reemplaza a parseStmtList
    Exp* parseCExp();
    Exp* parseRel();               // la antigua parseCExp (comparación <=)
    Exp* parseBExp();
    Exp* parseExpr();
    Exp* parseTerm();
    Exp* parseFactor();
};

#endif // PARSER_H
