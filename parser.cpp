#include <iostream>
#include <stdexcept>
#include "token.h"
#include "scanner.h"
#include "ast.h"
#include "parser.h"

using namespace std;

// Inicializa el parser leyendo el primer token
Parser::Parser(Scanner* sc) : scanner(sc) {
    previous = nullptr;
    current = scanner->nextToken();
    if (current->type == Token::ERR) {
        throw runtime_error("Error léxico");
    }
}

// Si el token actual es del tipo dado, avanza y devuelve true
bool Parser::match(Token::Type ttype) {
    if (check(ttype)) {
        advance();
        return true;
    }
    return false;
}

// Indica si el token actual es del tipo dado, sin avanzar
bool Parser::check(Token::Type ttype) {
    if (isAtEnd()) return false;
    return current->type == ttype;
}

// Avanza al siguiente token
bool Parser::advance() {
    if (!isAtEnd()) {
        Token* temp = current;
        if (previous) delete previous;
        current = scanner->nextToken();
        previous = temp;

        if (check(Token::ERR)) {
            throw runtime_error("Error lexico");
        }
        return true;
    }
    return false;
}

// Indica si se llegó al final de la entrada
bool Parser::isAtEnd() {
    return current->type == Token::END;
}

// Programa ::= FunDec+
Programa* Parser::parseProgram() {
    Programa* p = new Programa();
    if (!check(Token::FUN)) {
        throw runtime_error("Error sintáctico: el programa debe empezar con una función (fun)");
    }
    while (check(Token::FUN)) {
        p->funciones.push_back(parseFunDec());
    }
    if (!isAtEnd()) {
        throw runtime_error("Error sintáctico: se esperaba 'fun' o fin de archivo");
    }

    cout << "Parseo exitoso" << endl;
    return p;
}

// tipo ::= int | bool
string Parser::parseTipo() {
    if (!match(Token::ID) || (previous->text != "int" && previous->text != "bool")) {
        throw runtime_error("Error sintáctico: se esperaba un tipo (int o bool)");
    }
    return previous->text;
}

// FunDec ::= fun tipo id '(' [ tipo id (',' tipo id)* ] ')' Body endfun
FunDec* Parser::parseFunDec() {
    match(Token::FUN);
    FunDec* f = new FunDec();
    f->tipo = parseTipo();
    if (!match(Token::ID)) {
        throw runtime_error("Error sintáctico: se esperaba el nombre de la función");
    }
    f->nombre = previous->text;
    if (!match(Token::LPAREN)) {
        throw runtime_error("Error sintáctico: se esperaba '(' después de " + f->nombre);
    }
    if (!check(Token::RPAREN)) {
        do {
            f->parametros_tipos.push_back(parseTipo());
            if (!match(Token::ID)) {
                throw runtime_error("Error sintáctico: se esperaba el nombre del parámetro");
            }
            f->parametros_id.push_back(previous->text);
        } while (match(Token::COMMA));
    }
    if (!match(Token::RPAREN)) {
        throw runtime_error("Error sintáctico: se esperaba ')' en la función " + f->nombre);
    }
    f->cuerpo = parseBody();
    if (!match(Token::ENDFUN)) {
        throw runtime_error("Error sintáctico: se esperaba 'endfun' al final de " + f->nombre);
    }
    return f;
}

// Body ::= VarDec* StmtList
Body* Parser::parseBody() {
    Body* b = new Body();
    while (check(Token::VAR)) {
        b->declaraciones.push_back(parseVarDec());
    }
    for (Stmt* s : parseStmtList()) {
        b->sentencias.push_back(s);
    }
    return b;
}

// VarDec ::= var tipo id (',' id)* ';'      tipo ::= int | bool
VarDec* Parser::parseVarDec() {
    match(Token::VAR);
    VarDec* d = new VarDec();
    d->tipo = parseTipo();
    do {
        if (!match(Token::ID)) {
            throw runtime_error("Error sintáctico: se esperaba un nombre de variable");
        }
        d->variables.push_back(previous->text);
    } while (match(Token::COMMA));
    if (!match(Token::SEMICOL)) {
        throw runtime_error("Error sintáctico: se esperaba ';' al final de la declaración");
    }
    return d;
}

// StmtList ::= Stmt (';' Stmt)*
list<Stmt*> Parser::parseStmtList() {
    list<Stmt*> slist;
    slist.push_back(parseStmt());
    while (match(Token::SEMICOL)) {
        slist.push_back(parseStmt());
    }
    return slist;
}

// Stmt ::= id '=' CExp | print '(' CExp ')'
//        | if CExp then StmtList [ else StmtList ] endif
//        | while CExp do StmtList enddo
//        | do StmtList while CExp
//        | break
//        | return CExp
Stmt* Parser::parseStmt() {
    Exp* e;
    if (match(Token::ID)) {
        string variable = previous->text;
        match(Token::ASSIGN);
        e = parseCExp();
        return new AssignStmt(variable, e);
    }
    else if (match(Token::PRINT)) {
        match(Token::LPAREN);
        e = parseCExp();
        match(Token::RPAREN);
        return new PrintStmt(e);
    }
    else if (match(Token::IF)) {
        e = parseCExp();
        if (!match(Token::THEN)) {
            throw runtime_error("Error sintáctico: se esperaba 'then'");
        }
        list<Stmt*> thenList = parseStmtList();
        list<Stmt*> elseList;
        if (match(Token::ELSE)) {
            elseList = parseStmtList();
        }
        if (!match(Token::ENDIF)) {
            throw runtime_error("Error sintáctico: se esperaba 'endif'");
        }
        return new IfStmt(e, thenList, elseList);
    }
    else if (match(Token::WHILE)) {
        e = parseCExp();
        if (!match(Token::DO)) {
            throw runtime_error("Error sintáctico: se esperaba 'do'");
        }
        loopDepth++;
        list<Stmt*> body = parseStmtList();
        loopDepth--;
        if (!match(Token::ENDDO)) {
            throw runtime_error("Error sintáctico: se esperaba 'enddo'");
        }
        return new WhileStmt(e, body);
    }
    else if (match(Token::DO)) {
        loopDepth++;
        list<Stmt*> body = parseStmtList();
        loopDepth--;
        if (!match(Token::WHILE)) {
            throw runtime_error("Error sintáctico: se esperaba 'while' al final del do");
        }
        e = parseCExp();
        return new DoWhileStmt(body, e);
    }
    else if (match(Token::RETURN)) {
        ReturnStmt* r = new ReturnStmt();
        r->exp = parseCExp();
        return r;
    }
    else if (match(Token::BREAK)) {
        if (loopDepth == 0) {
            throw runtime_error("Error sintáctico: 'break' fuera de un bucle");
        }
        return new BreakStmt();
    }
    else {
        throw runtime_error("Error sintáctico");
    }
}

// CExp ::= BExp [ <= BExp ]
Exp* Parser::parseCExp() {
    Exp* l = parseBExp();
    if (match(Token::LE)) {
        Exp* r = parseBExp();
        l = new BinaryExp(l, r, LE_OP);
    }
    return l;
}

// BExp ::= expr { (+ | -) expr }*
Exp* Parser::parseBExp() {
    Exp* l = parseExpr();
    while (match(Token::PLUS) || match(Token::MINUS)) {
        BinaryOp op;
        if (previous->type == Token::PLUS) {
            op = PLUS_OP;
        }
        else {
            op = MINUS_OP;
        }
        Exp* r = parseExpr();
        l = new BinaryExp(l, r, op);
    }
    return l;
}

// expr ::= term { (* | /) term }*
Exp* Parser::parseExpr() {
    Exp* l = parseTerm();
    while (match(Token::MUL) || match(Token::DIV)) {
        BinaryOp op;
        if (previous->type == Token::MUL) {
            op = MUL_OP;
        }
        else {
            op = DIV_OP;
        }
        Exp* r = parseTerm();
        l = new BinaryExp(l, r, op);
    }
    return l;
}

// term ::= factor [ ** factor ]
Exp* Parser::parseTerm() {
    Exp* l = parseFactor();
    if (match(Token::POW)) {
        Exp* r = parseFactor();
        l = new BinaryExp(l, r, POW_OP);
    }
    return l;
}

// factor ::= number | true | false | '(' CExp ')' | id | id '(' [CExp (',' CExp)*] ')' | ifexp '(' CExp ',' CExp ',' CExp ')'
Exp* Parser::parseFactor() {
    if (match(Token::NUM)) {
        return new NumberExp(stoi(previous->text));
    }
    else if (match(Token::TRUE)) {
        return new NumberExp(1, true);
    }
    else if (match(Token::FALSE)) {
        return new NumberExp(0, true);
    }
    else if (match(Token::LPAREN)) {
        Exp* e = parseCExp();
        match(Token::RPAREN);
        return e;
    }
    else if (match(Token::ID)) {
        string nombre = previous->text;
        if (match(Token::LPAREN)) {                 // llamada: id '(' [CExp (',' CExp)*] ')'
            FCallExp* call = new FCallExp();
            call->nombre = nombre;
            if (!check(Token::RPAREN)) {
                do {
                    call->argumentos.push_back(parseCExp());
                } while (match(Token::COMMA));
            }
            if (!match(Token::RPAREN)) {
                throw runtime_error("Error sintáctico: se esperaba ')' en la llamada a " + nombre);
            }
            return call;
        }
        return new IdExp(nombre);
    }
    else if (match(Token::MAX) || match(Token::MIN)) {
        MaxMinExp* m = new MaxMinExp(previous->type == Token::MAX);
        string nombre = previous->text;
        if (!match(Token::LPAREN)) throw runtime_error("Error sintáctico: se esperaba '(' tras " + nombre);
        do {
            m->argumentos.push_back(parseCExp());
        } while (match(Token::COMMA));
        if (!match(Token::RPAREN)) throw runtime_error("Error sintáctico: se esperaba ')' en " + nombre);
        return m;
    }
    else if (match(Token::COUNT)) {
        CountExp* c = new CountExp();
        if (!match(Token::LPAREN)) throw runtime_error("Error sintáctico: se esperaba '(' tras count");
        if (!check(Token::RPAREN)) {
            do {
                c->argumentos.push_back(parseCExp());
            } while (match(Token::COMMA));
        }
        if (!match(Token::RPAREN)) throw runtime_error("Error sintáctico: se esperaba ')' en count");
        return c;
    }
    else if (match(Token::IFEXP)) {
        if (!match(Token::LPAREN)) throw runtime_error("Error sintáctico: se esperaba '(' tras ifexp");
        Exp* c = parseCExp();
        if (!match(Token::COMMA)) throw runtime_error("Error sintáctico: se esperaba ','");
        Exp* t = parseCExp();
        if (!match(Token::COMMA)) throw runtime_error("Error sintáctico: se esperaba ','");
        Exp* f = parseCExp();
        if (!match(Token::RPAREN)) throw runtime_error("Error sintáctico: se esperaba ')'");
        return new IfExp(c, t, f);
    }
    else {
        throw runtime_error("Error sintáctico");
    }
}
