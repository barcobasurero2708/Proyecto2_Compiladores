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
        throw runtime_error("Error léxico: token inválido '" + current->text + "'");
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
            throw runtime_error("Error léxico: token inválido '" + current->text + "'");
        }
        return true;
    }
    return false;
}

// Indica si se llegó al final de la entrada
bool Parser::isAtEnd() {
    return current->type == Token::END;
}

bool Parser::checkTipo() {
    return check(Token::TINT) || check(Token::TBOOL) || check(Token::TCHAR)
        || check(Token::TFLOAT) || check(Token::TSTRING);
}

// Verifica que el token actual sea del tipo dado y lo consume; si no, lanza el error
void Parser::consume(Token::Type ttype, const string& mensaje) {
    if (!match(ttype)) {
        throw runtime_error("Error sintáctico: " + mensaje);
    }
}

// Resuelve las secuencias de escape de un literal char o string (el texto viene sin comillas)
static string desescapar(const string& crudo) {
    string r;
    for (size_t i = 0; i < crudo.size(); i++) {
        if (crudo[i] != '\\') {
            r += crudo[i];
            continue;
        }
        if (++i >= crudo.size()) {
            throw runtime_error("Error sintáctico: secuencia de escape incompleta");
        }
        switch (crudo[i]) {
            case 'n':  r += '\n'; break;
            case 't':  r += '\t'; break;
            case '0':  r += '\0'; break;
            case '\\': r += '\\'; break;
            case '\'': r += '\''; break;
            case '"':  r += '"'; break;
            default:
                throw runtime_error(string("Error sintáctico: secuencia de escape desconocida '\\") + crudo[i] + "'");
        }
    }
    return r;
}

// Programa ::= [ using namespace id ';' ] FunDec+
Programa* Parser::parseProgram() {
    Programa* p = new Programa();
    consume(Token::INCLUDE, "el programa debe empezar con '#include <iostream>'");
    consume(Token::USING, "se esperaba 'using namespace std;' después de #include <iostream>");
    consume(Token::NAMESPACE, "se esperaba 'namespace' después de 'using'");
    consume(Token::ID, "se esperaba 'std' después de 'using namespace'");
    if (previous->text != "std") {
        throw runtime_error("Error sintáctico: solo se admite 'using namespace std;'");
    }
    consume(Token::SEMICOL, "se esperaba ';' después de 'using namespace std'");
    if (!checkTipo()) {
        throw runtime_error("Error sintáctico: se esperaba al menos una función (tipo nombre(...))");
    }
    while (checkTipo()) {
        p->funciones.push_back(parseFunDec());
    }
    if (!isAtEnd()) {
        throw runtime_error("Error sintáctico: se esperaba una función o fin de archivo");
    }

    cout << "Parseo exitoso" << endl;
    return p;
}

// tipo ::= int | bool | char | float | string
string Parser::parseTipo() {
    if (!checkTipo()) {
        throw runtime_error("Error sintáctico: se esperaba un tipo (int, bool, char, float o string)");
    }
    advance();
    if (previous->type == Token::TFLOAT) return "float";
    return previous->text;
}

// FunDec ::= tipo id '(' [ tipo id (',' tipo id)* ] ')' '{' Body '}'
FunDec* Parser::parseFunDec() {
    FunDec* f = new FunDec();
    f->tipo = parseTipo();
    consume(Token::ID, "se esperaba el nombre de la función");
    f->nombre = previous->text;
    consume(Token::LPAREN, "se esperaba '(' después de " + f->nombre);
    if (!check(Token::RPAREN)) {
        do {
            f->parametros_tipos.push_back(parseTipo());
            consume(Token::ID, "se esperaba el nombre del parámetro");
            f->parametros_id.push_back(previous->text);
        } while (match(Token::COMMA));
    }
    consume(Token::RPAREN, "se esperaba ')' en la función " + f->nombre);
    consume(Token::LBRACE, "se esperaba '{' al inicio de la función " + f->nombre);
    f->cuerpo = parseBody();
    consume(Token::RBRACE, "se esperaba '}' al final de la función " + f->nombre);
    return f;
}

// Body ::= ( VarDec | Stmt )*      (termina al encontrar '}')
// Se conserva el orden para que una variable no sea visible antes de declararse.
Body* Parser::parseBody() {
    Body* b = new Body();
    while (!check(Token::RBRACE)) {
        if (isAtEnd()) {
            throw runtime_error("Error sintáctico: falta '}' al final de la función");
        }
        if (checkTipo()) {
            b->sentencias.push_back(parseVarDec());
        }
        else {
            b->sentencias.push_back(parseStmt());
        }
    }
    return b;
}

// VarDec ::= tipo id [ '=' CExp ] (',' id [ '=' CExp ])* ';'
VarDec* Parser::parseVarDec() {
    VarDec* d = new VarDec();
    d->tipo = parseTipo();
    do {
        consume(Token::ID, "se esperaba un nombre de variable");
        string nombre = previous->text;
        d->variables.push_back(nombre);
        d->inicializadores.push_back(match(Token::ASSIGN) ? parseCExp() : nullptr);
    } while (match(Token::COMMA));
    consume(Token::SEMICOL, "se esperaba ';' al final de la declaración");
    return d;
}

// Block ::= '{' Stmt* '}' | Stmt
list<Stmt*> Parser::parseBlock() {
    list<Stmt*> slist;
    if (match(Token::LBRACE)) {
        while (!check(Token::RBRACE)) {
            if (isAtEnd()) {
                throw runtime_error("Error sintáctico: falta '}' al final del bloque");
            }
            slist.push_back(parseStmt());
        }
        match(Token::RBRACE);
    }
    else {
        slist.push_back(parseStmt());
    }
    return slist;
}

// Condition ::= '(' CExp ')'
Exp* Parser::parseCondition() {
    consume(Token::LPAREN, "se esperaba '(' antes de la condición");
    Exp* e = parseCExp();
    consume(Token::RPAREN, "se esperaba ')' después de la condición");
    return e;
}

// Stmt ::= id '=' CExp ';'
//        | cout '<<' CExp '<<' endl ';'
//        | if Condition Block [ else Block ]
//        | while Condition Block
//        | do Block while Condition ';'
//        | break ';'
//        | return CExp ';'
Stmt* Parser::parseStmt() {
    Exp* e;
    if (match(Token::ID)) {
        string variable = previous->text;
        consume(Token::ASSIGN, "se esperaba '=' después de " + variable);
        e = parseCExp();
        consume(Token::SEMICOL, "se esperaba ';' al final de la asignación");
        return new AssignStmt(variable, e);
    }
    else if (match(Token::COUT)) {
        consume(Token::SHL, "se esperaba '<<' después de cout");
        vector<Exp*> expresiones;
        do {
            expresiones.push_back(parseCExp());
            consume(Token::SHL, "se esperaba '<<' antes de endl");
        } while (!check(Token::ENDL));
        consume(Token::ENDL, "se esperaba 'endl'");
        consume(Token::SEMICOL, "se esperaba ';' al final de cout");
        return new PrintStmt(expresiones);
    }
    else if (match(Token::IF)) {
        e = parseCondition();
        list<Stmt*> thenList = parseBlock();
        list<Stmt*> elseList;
        if (match(Token::ELSE)) {
            elseList = parseBlock();      // "else if" funciona solo: el bloque es un if
        }
        return new IfStmt(e, thenList, elseList);
    }
    else if (match(Token::WHILE)) {
        e = parseCondition();
        loopDepth++;
        list<Stmt*> body = parseBlock();
        loopDepth--;
        return new WhileStmt(e, body);
    }
    else if (match(Token::DO)) {
        loopDepth++;
        list<Stmt*> body = parseBlock();
        loopDepth--;
        consume(Token::WHILE, "se esperaba 'while' al final del do");
        e = parseCondition();
        consume(Token::SEMICOL, "se esperaba ';' después del do-while");
        return new DoWhileStmt(body, e);
    }
    else if (match(Token::RETURN)) {
        ReturnStmt* r = new ReturnStmt();
        r->exp = parseCExp();
        consume(Token::SEMICOL, "se esperaba ';' después de return");
        return r;
    }
    else if (match(Token::BREAK)) {
        if (loopDepth == 0) {
            throw runtime_error("Error sintáctico: 'break' fuera de un bucle");
        }
        consume(Token::SEMICOL, "se esperaba ';' después de break");
        return new BreakStmt();
    }
    else if (checkTipo()) {
        throw runtime_error("Error sintáctico: solo se permiten declaraciones al nivel de la función, no dentro de if/while/do");
    }
    else {
        throw runtime_error("Error sintáctico");
    }
}

// CExp ::= Rel [ '?' CExp ':' CExp ]
Exp* Parser::parseCExp() {
    Exp* c = parseEq();
    if (match(Token::QUESTION)) {
        Exp* t = parseCExp();
        consume(Token::COLON, "se esperaba ':' en el operador ?:");
        Exp* f = parseCExp();
        return new IfExp(c, t, f);
    }
    return c;
}

// Eq ::= Rel { ('==' | '!=') Rel }
Exp* Parser::parseEq() {
    Exp* l = parseRel();
    while (match(Token::EQ) || match(Token::NE)) {
        BinaryOp op = previous->type == Token::EQ ? EQ_OP : NE_OP;
        Exp* r = parseRel();
        l = new BinaryExp(l, r, op);
    }
    return l;
}

// Rel ::= BExp { ('<' | '<=' | '>' | '>=') BExp }
Exp* Parser::parseRel() {
    Exp* l = parseBExp();
    while (match(Token::LT) || match(Token::LE) || match(Token::GT) || match(Token::GE)) {
        BinaryOp op;
        switch (previous->type) {
            case Token::LT: op = LT_OP; break;
            case Token::LE: op = LE_OP; break;
            case Token::GT: op = GT_OP; break;
            default:        op = GE_OP; break;
        }
        Exp* r = parseBExp();
        l = new BinaryExp(l, r, op);
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

// term ::= '-' term | factor
Exp* Parser::parseTerm() {
    if (match(Token::MINUS)) {
        Exp* e = parseTerm();
        return new BinaryExp(new NumberExp(0), e, MINUS_OP);   // -x se trata como 0 - x
    }
    return parseFactor();
}

// factor ::= number | true | false | '(' CExp ')' | id | id '(' [CExp (',' CExp)*] ')'
Exp* Parser::parseFactor() {
    if (match(Token::NUM)) {
        return new NumberExp(stoi(previous->text));
    }
    else if (match(Token::FLOATNUM)) {
        return NumberExp::crearFloat(stod(previous->text));
    }
    else if (match(Token::CHARLIT)) {
        string crudo = previous->text;
        string valor = desescapar(crudo.substr(1, crudo.size() - 2));
        if (valor.size() != 1) {
            throw runtime_error("Error sintáctico: un literal char debe tener un solo carácter: " + crudo);
        }
        return NumberExp::crearChar(valor[0]);
    }
    else if (match(Token::STRLIT)) {
        string crudo = previous->text;
        string valor = desescapar(crudo.substr(1, crudo.size() - 2));
        if (valor.find('\0') != string::npos) {
            throw runtime_error("Error sintáctico: no se admite '\\0' dentro de un string");
        }
        return NumberExp::crearString(valor);
    }
    else if (match(Token::TRUE)) {
        return new NumberExp(1, true);
    }
    else if (match(Token::FALSE)) {
        return new NumberExp(0, true);
    }
    else if (match(Token::LPAREN)) {
        Exp* e = parseCExp();
        consume(Token::RPAREN, "se esperaba ')' al cerrar la expresión");
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
    else {
        throw runtime_error("Error sintáctico");
    }
}
