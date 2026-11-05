#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <algorithm>
#include <memory>
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
        || check(Token::TFLOAT) || check(Token::TDOUBLE) || check(Token::TLONG)
        || check(Token::TSTRING)
        || (check(Token::ID) && tiposEstructura.count(current->text));
}

bool Parser::checkDeclaracion() {
    return checkTipo() && (!check(Token::ID) || scanner->peekTipo() == Token::ID);
}

// Verifica que el token actual sea del tipo dado y lo consume; si no, lanza el error
void Parser::consume(Token::Type ttype, const string& mensaje) {
    if (!match(ttype)) {
        string encontrado = isAtEnd() ? "fin de archivo" : "'" + current->text + "'";
        throw runtime_error("Error sintáctico: " + mensaje + " (se encontró " + encontrado + ")");
    }
}

// Consume un identificador (nombre de variable, parámetro o función) y lo devuelve
string Parser::consumirNombre(const string& mensaje) {
    // Cualquier palabra (if, for, int, void, ...) que no sea un identificador es reservada
    bool esPalabra = !isAtEnd() && !current->text.empty()
                     && (isalpha((unsigned char)current->text[0]) || current->text[0] == '_');
    if (!check(Token::ID) && esPalabra) {
        throw runtime_error("Error sintáctico: '" + current->text
                            + "' es una palabra reservada y no puede usarse como nombre");
    }
    consume(Token::ID, mensaje);
    if (previous->text.rfind("std::", 0) == 0) {
        throw runtime_error("Error sintáctico: no se puede declarar el nombre calificado " + previous->text);
    }
    return previous->text;
}

// cout, endl y string pertenecen a std: se exige la cabecera que los declara y
// el prefijo std:: o 'using namespace std;' antes de usarlos, como en C++
void Parser::verificarStd(const string& nombre, const string& texto, const string& cabecera) {
    bool declarado = includes.count(cabecera) || (cabecera == "string" && includes.count("iostream"));
    if (!declarado) {
        throw runtime_error("Error sintáctico: '" + texto + "' no está declarado (falta #include <"
                            + cabecera + ">)");
    }
    if (texto.rfind("std::", 0) != 0 && !usingStd) {
        throw runtime_error("Error sintáctico: '" + nombre + "' no está declarado; use std::" + nombre
                            + " o 'using namespace std;'");
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

// Programa ::= { "#include" "<" cabecera ">" | "using" "namespace" "std" ";" | FunDec }
// Como en C++, 'using namespace std;' solo afecta a lo que viene después.
Programa* Parser::parseProgram() {
    Programa* p = new Programa();
    while (!isAtEnd()) {
        if (match(Token::INCLUDE)) {
            string cabecera = previous->text;
            if (cabecera != "iostream" && cabecera != "string") {
                throw runtime_error("Error sintáctico: solo se admiten #include <iostream> y #include <string>");
            }
            includes.insert(cabecera);
        }
        else if (match(Token::USING)) {
            consume(Token::NAMESPACE, "se esperaba 'namespace' después de 'using'");
            consume(Token::ID, "se esperaba el nombre del namespace");
            if (previous->text != "std") {
                throw runtime_error("Error sintáctico: '" + previous->text + "' no es un namespace");
            }
            if (includes.empty()) {
                throw runtime_error("Error sintáctico: 'std' no es un namespace declarado (falta #include <iostream>)");
            }
            consume(Token::SEMICOL, "se esperaba ';' después de 'using namespace std'");
            usingStd = true;
        }
        else if (match(Token::STRUCT)) {
            parseEstructura(p);
        }
        else if (checkTipo() || check(Token::TVOID)) {
            p->funciones.push_back(parseFunDec());
        }
        else {
            throw runtime_error("Error sintáctico: se esperaba una función, struct, #include o 'using namespace std;' (se encontró '"
                                + current->text + "')");
        }
    }

    cout << "Parseo exitoso" << endl;
    return p;
}

void Parser::parseEstructura(Programa* programa) {
    string nombre = consumirNombre("se esperaba el nombre del struct");
    if (tiposEstructura.count(nombre)) {
        throw runtime_error("Error sintáctico: struct redeclarado: " + nombre);
    }
    consume(Token::LBRACE, "se esperaba '{' después de " + nombre);
    vector<string> campos;
    while (!check(Token::RBRACE)) {
        consume(Token::TINT, "solo se admiten campos int en " + nombre);
        do {
            string campo = consumirNombre("se esperaba el nombre del campo");
            if (find(campos.begin(), campos.end(), campo) != campos.end()) {
                throw runtime_error("Error sintáctico: campo repetido: " + campo);
            }
            campos.push_back(campo);
            if (campos.size() > 2) {
                throw runtime_error("Error sintáctico: solo se admiten hasta dos campos int");
            }
        } while (match(Token::COMMA));
        consume(Token::SEMICOL, "se esperaba ';' después de los campos");
    }
    if (campos.empty()) throw runtime_error("Error sintáctico: el struct necesita un campo int");
    consume(Token::RBRACE, "se esperaba '}' al cerrar el struct");
    consume(Token::SEMICOL, "se esperaba ';' después del struct");
    tiposEstructura.insert(nombre);
    programa->estructuras.emplace(nombre, campos);
}

StructExp* Parser::parseInicializadorStruct(const string& nombre) {
    auto valor = make_unique<StructExp>(nombre);
    consume(Token::LBRACE, "se esperaba '{' al inicializar " + nombre);
    while (!check(Token::RBRACE)) {
        valor->valores.push_back(parseCExp());
        if (!match(Token::COMMA)) break;
    }
    consume(Token::RBRACE, "se esperaba '}' al inicializar " + nombre);
    return valor.release();
}

// tipo ::= int | bool | char | float | double | long [long] [int] | string
string Parser::parseTipo() {
    if (!checkTipo()) {
        throw runtime_error("Error sintáctico: se esperaba un tipo básico o un struct declarado");
    }
    advance();
    switch (previous->type) {
        case Token::TFLOAT:  return "float";
        case Token::TDOUBLE: return "double";
        case Token::TLONG:
            match(Token::TLONG);          // long long (64 bits, igual que long en x86-64 Linux)
            match(Token::TINT);           // long int / long long int
            return "long";
        case Token::TSTRING:
            verificarStd("string", previous->text, "string");
            return "string";
        default:
            return previous->text;        // int, bool, char
    }
}

// FunDec ::= ( tipo | void ) id '(' [ void | Param (',' Param)* ] ')' ( '{' Body '}' | ';' )
// Param  ::= tipo [ id ] { '[' [ NUM ] ']' }
FunDec* Parser::parseFunDec() {
    FunDec* f = new FunDec();
    f->tipo = match(Token::TVOID) ? "void" : parseTipo();
    string etiquetaOperador;
    if (match(Token::OPERATOR)) {
        switch (current->type) {
            case Token::PLUS: case Token::MINUS: case Token::MUL: case Token::DIV:
            case Token::MOD: case Token::EQ: case Token::NE: case Token::LT:
            case Token::LE: case Token::GT: case Token::GE:
                f->operador = current->text;
                etiquetaOperador = "operador." + to_string(current->type);
                advance();
                break;
            default:
                throw runtime_error("Error sintáctico: sobrecarga no soportada para '" + current->text + "'");
        }
        f->nombre = "operator" + f->operador;
    }
    else {
        f->nombre = consumirNombre("se esperaba el nombre de la función");
    }
    consume(Token::LPAREN, "se esperaba '(' después de " + f->nombre);
    if (match(Token::TVOID)) {
        // f(void): lista de parámetros vacía
    }
    else if (!check(Token::RPAREN)) {
        do {
            string tipo = parseTipo();
            f->parametros_id.push_back(check(Token::ID) || check(Token::RESERVED)
                                       ? consumirNombre("se esperaba el nombre del parámetro") : "");
            // Un parámetro arreglo es un puntero a su primer elemento: el tamaño de la
            // primera dimensión no importa (int a[5] es lo mismo que int a[])
            vector<int> dims = parseDimensiones();
            if (!dims.empty()) {
                if (!f->operador.empty() || tiposEstructura.count(tipo)) {
                    throw runtime_error("Error sintáctico: parámetro arreglo no soportado en esta firma");
                }
                tipo += "[]";
                for (size_t k = 1; k < dims.size(); k++) tipo += "[" + to_string(dims[k]) + "]";
            }
            f->parametros_tipos.push_back(tipo);
        } while (match(Token::COMMA));
    }
    consume(Token::RPAREN, "se esperaba ')' en la función " + f->nombre);
    if (!f->operador.empty()) {
        for (const string& tipo : f->parametros_tipos) etiquetaOperador += "." + tipo;
        f->nombre = etiquetaOperador;
    }
    if (match(Token::SEMICOL)) {
        return f;                                    // prototipo: sin cuerpo
    }
    consume(Token::LBRACE, "se esperaba '{' o ';' después de la cabecera de " + f->nombre);
    tipoRetorno = f->tipo;
    f->cuerpo = parseBody();
    consume(Token::RBRACE, "se esperaba '}' al final de la función " + f->nombre);
    return f;
}

// Body ::= Stmt*      (termina al encontrar '}')
Body* Parser::parseBody() {
    Body* b = new Body();
    while (!check(Token::RBRACE)) {
        if (isAtEnd()) {
            throw runtime_error("Error sintáctico: falta '}' al final de la función");
        }
        b->sentencias.push_back(parseStmt());
    }
    return b;
}

// Dims ::= { '[' [ NUM ] ']' }      (-1 = tamaño omitido; solo la primera dimensión puede omitirse)
vector<int> Parser::parseDimensiones() {
    vector<int> dims;
    while (match(Token::LBRACKET)) {
        if (match(Token::RBRACKET)) {
            if (!dims.empty()) {
                throw runtime_error("Error sintáctico: solo la primera dimensión de un arreglo puede omitirse");
            }
            dims.push_back(-1);
            continue;
        }
        consume(Token::NUM, "el tamaño de un arreglo debe ser un número entero");
        long long n = stoll(previous->text);
        if (n <= 0 || n > 100000000) {
            throw runtime_error("Error sintáctico: tamaño de arreglo inválido: " + previous->text);
        }
        dims.push_back((int)n);
        consume(Token::RBRACKET, "se esperaba ']'");
    }
    return dims;
}

// Lista ::= '{' [ ( Lista | CExp ) { ',' ( Lista | CExp ) } [ ',' ] ] '}'
// Guarda los elementos en orden (fila por fila) a partir de 'inicio'. Como en C++,
// se pueden omitir las llaves internas y los elementos que faltan valen 0.
void Parser::parseLista(vector<int>& dims, size_t nivel, size_t inicio, vector<Exp*>& elementos) {
    size_t paso = 1;                                    // elementos de cada subarreglo de este nivel
    for (size_t k = nivel + 1; k < dims.size(); k++) paso *= dims[k];
    size_t limite = dims[nivel] < 0 ? SIZE_MAX : dims[nivel] * paso;
    consume(Token::LBRACE, "se esperaba '{' en el inicializador del arreglo");
    size_t p = 0;
    do {
        if (check(Token::RBRACE)) break;               // lista vacía o coma final
        if (check(Token::LBRACE)) {
            if (nivel + 1 >= dims.size()) {
                throw runtime_error("Error sintáctico: demasiadas llaves en el inicializador del arreglo");
            }
            p = (p + paso - 1) / paso * paso;           // empieza el siguiente subarreglo
            if (p >= limite) {
                throw runtime_error("Error sintáctico: demasiados inicializadores para el arreglo");
            }
            parseLista(dims, nivel + 1, inicio + p, elementos);
            p += paso;
        }
        else {
            if (p >= limite) {
                throw runtime_error("Error sintáctico: demasiados inicializadores para el arreglo");
            }
            if (elementos.size() <= inicio + p) elementos.resize(inicio + p + 1, nullptr);
            elementos[inicio + p] = parseCExp();
            p++;
        }
    } while (match(Token::COMMA));
    consume(Token::RBRACE, "se esperaba '}' al final del inicializador del arreglo");
}

// VarDec ::= tipo Declarador (',' Declarador)* ';'
// Declarador ::= id [ '=' CExp ]  |  id Dims [ '=' Lista ]
VarDec* Parser::parseVarDec() {
    VarDec* d = new VarDec();
    d->tipo = parseTipo();
    do {
        string nombre = consumirNombre("se esperaba un nombre de variable");
        vector<int> dims = parseDimensiones();
        Exp* init = nullptr;
        vector<Exp*> elementos;
        bool esStruct = tiposEstructura.count(d->tipo);
        if (esStruct && !dims.empty()) {
            throw runtime_error("Error sintáctico: no se admiten arreglos de struct");
        }
        if (dims.empty()) {
            bool asignacion = match(Token::ASSIGN);
            if (esStruct && check(Token::LBRACE)) init = parseInicializadorStruct(d->tipo);
            else if (asignacion) init = parseCExp();
        }
        else if (match(Token::ASSIGN)) {
            if (!check(Token::LBRACE)) {
                throw runtime_error("Error sintáctico: el arreglo " + nombre + " se inicializa con { ... }");
            }
            parseLista(dims, 0, 0, elementos);
            if (dims[0] < 0) {
                // int a[] = {1, 2, 3}: el tamaño sale de la cantidad de elementos
                size_t fila = 1;
                for (size_t k = 1; k < dims.size(); k++) fila *= dims[k];
                if (elementos.empty()) {
                    throw runtime_error("Error sintáctico: el arreglo " + nombre + " no puede tener tamaño 0");
                }
                dims[0] = (elementos.size() + fila - 1) / fila;
            }
        }
        else if (dims[0] < 0) {
            throw runtime_error("Error sintáctico: falta el tamaño del arreglo " + nombre);
        }
        if (!dims.empty()) {
            size_t total = 1;
            for (int n : dims) total *= n;
            if (total > 100000000) {
                throw runtime_error("Error sintáctico: el arreglo " + nombre + " es demasiado grande");
            }
            elementos.resize(total, nullptr);
        }
        d->variables.push_back(nombre);
        d->inicializadores.push_back(init);
        d->dimensiones.push_back(dims);
        d->listas.push_back(elementos);
    } while (match(Token::COMMA));
    consume(Token::SEMICOL, "se esperaba ';' al final de la declaración");
    return d;
}

// Condition ::= '(' CExp ')'
Exp* Parser::parseCondition() {
    consume(Token::LPAREN, "se esperaba '(' antes de la condición");
    Exp* e = parseCExp();
    consume(Token::RPAREN, "se esperaba ')' después de la condición");
    return e;
}

// Stmt ::= VarDec
//        | '{' Stmt* '}'
//        | ';'
//        | cout { '<<' ( endl | BExp ) } ';'
//        | if Condition Stmt [ else Stmt ]
//        | while Condition Stmt
//        | do Stmt while Condition ';'
//        | for '(' [ VarDec | Simple ';' | ';' ] [ CExp ] ';' [ Simple ] ')' Stmt
//        | break ';'
//        | continue ';'
//        | return [ CExp ] ';'
//        | Simple ';'
Stmt* Parser::parseStmt() {
    Exp* e;
    if (checkDeclaracion()) {
        return parseVarDec();
    }
    else if (match(Token::LBRACE)) {
        BlockStmt* b = new BlockStmt();
        while (!check(Token::RBRACE)) {
            if (isAtEnd()) {
                throw runtime_error("Error sintáctico: falta '}' al final del bloque");
            }
            b->sentencias.push_back(parseStmt());
        }
        match(Token::RBRACE);
        return b;
    }
    else if (match(Token::SEMICOL)) {
        return new BlockStmt();                    // sentencia vacía
    }
    else if (match(Token::COUT)) {
        verificarStd("cout", previous->text, "iostream");
        // En C++ '<<' tiene más precedencia que las comparaciones y el '?:',
        // así que cada operando es una expresión aditiva (cout << a < b no compila)
        vector<Exp*> expresiones;
        while (match(Token::SHL)) {
            if (match(Token::ENDL)) {
                verificarStd("endl", previous->text, "iostream");
                expresiones.push_back(nullptr);
            }
            else {
                expresiones.push_back(parseBExp());
            }
        }
        consume(Token::SEMICOL, "se esperaba '<<' o ';' en cout");
        return new PrintStmt(expresiones);
    }
    else if (match(Token::IF)) {
        e = parseCondition();
        list<Stmt*> thenList = {parseStmt()};
        list<Stmt*> elseList;
        if (match(Token::ELSE)) {
            elseList.push_back(parseStmt());      // "else if" funciona solo: la sentencia es un if
        }
        return new IfStmt(e, thenList, elseList);
    }
    else if (match(Token::WHILE)) {
        e = parseCondition();
        loopDepth++;
        list<Stmt*> body = {parseStmt()};
        loopDepth--;
        return new WhileStmt(e, body);
    }
    else if (match(Token::DO)) {
        loopDepth++;
        list<Stmt*> body = {parseStmt()};
        loopDepth--;
        consume(Token::WHILE, "se esperaba 'while' al final del do");
        e = parseCondition();
        consume(Token::SEMICOL, "se esperaba ';' después del do-while");
        return new DoWhileStmt(body, e);
    }
    else if (match(Token::FOR)) {
        consume(Token::LPAREN, "se esperaba '(' después de for");
        Stmt* init = nullptr;
        if (checkDeclaracion()) {
            init = parseVarDec();                  // ya consume el ';'
        }
        else if (!match(Token::SEMICOL)) {
            init = parseAsignacionOExp();
            consume(Token::SEMICOL, "se esperaba ';' después de la inicialización del for");
        }
        Exp* cond = check(Token::SEMICOL) ? nullptr : parseCExp();
        consume(Token::SEMICOL, "se esperaba ';' después de la condición del for");
        Stmt* actualizacion = check(Token::RPAREN) ? nullptr : parseAsignacionOExp();
        consume(Token::RPAREN, "se esperaba ')' al final del for");
        loopDepth++;
        list<Stmt*> body = {parseStmt()};
        loopDepth--;
        return new ForStmt(init, cond, actualizacion, body);
    }
    else if (match(Token::RETURN)) {
        ReturnStmt* r = new ReturnStmt();
        if (tiposEstructura.count(tipoRetorno) && check(Token::LBRACE)) {
            r->exp = parseInicializadorStruct(tipoRetorno);
        }
        else if (!check(Token::SEMICOL)) {
            r->exp = parseCExp();                  // 'return;' solo en funciones void
        }
        consume(Token::SEMICOL, "se esperaba ';' después de return");
        return r;
    }
    else if (match(Token::CONTINUE)) {
        if (loopDepth == 0) {
            throw runtime_error("Error sintáctico: 'continue' fuera de un bucle");
        }
        consume(Token::SEMICOL, "se esperaba ';' después de continue");
        return new ContinueStmt();
    }
    else if (match(Token::BREAK)) {
        if (loopDepth == 0) {
            throw runtime_error("Error sintáctico: 'break' fuera de un bucle");
        }
        consume(Token::SEMICOL, "se esperaba ';' después de break");
        return new BreakStmt();
    }
    else if (check(Token::RESERVED) || check(Token::TVOID)) {
        throw runtime_error("Error sintáctico: '" + current->text + "' no está soportado en este subconjunto de C++");
    }
    // Asignación o sentencia de expresión
    Stmt* s = parseAsignacionOExp();
    consume(Token::SEMICOL, "se esperaba ';' al final de la sentencia");
    return s;
}

// Simple ::= ( id | id '[' CExp ']' { '[' CExp ']' } ) '=' CExp  |  CExp      (sin el ';')
// Se usa en las sentencias y en la inicialización y actualización del for
Stmt* Parser::parseAsignacionOExp() {
    Exp* e = parseCExp();
    if (!match(Token::ASSIGN)) {
        return new ExprStmt(e);
    }
    Exp* valor;
    if (IdExp* destino = dynamic_cast<IdExp*>(e)) {
        string variable = destino->value;
        delete destino;
        valor = parseCExp();
        return new AssignStmt(variable, valor);
    }
    if (ArrayExp* elemento = dynamic_cast<ArrayExp*>(e)) {
        string variable = elemento->nombre;
        vector<Exp*> indices = elemento->indices;
        elemento->indices.clear();                 // los índices pasan a la asignación
        delete elemento;
        valor = parseCExp();
        return new AssignStmt(variable, indices, valor);
    }
    if (CampoExp* miembro = dynamic_cast<CampoExp*>(e)) {
        if (IdExp* objeto = dynamic_cast<IdExp*>(miembro->objeto)) {
            string variable = objeto->value;
            string campo = miembro->campo;
            delete miembro;
            auto* sentencia = new AssignStmt(variable, parseCExp());
            sentencia->campo = campo;
            return sentencia;
        }
    }
    delete e;
    throw runtime_error("Error sintáctico: se esperaba una variable, un elemento de arreglo o un campo asignable");
}

// CExp ::= Or [ '?' CExp ':' CExp ]
Exp* Parser::parseCExp() {
    Exp* c = parseOr();
    if (match(Token::QUESTION)) {
        Exp* t = parseCExp();
        consume(Token::COLON, "se esperaba ':' en el operador ?:");
        Exp* f = parseCExp();
        return new IfExp(c, t, f);
    }
    return c;
}

// Or ::= And { '||' And }
Exp* Parser::parseOr() {
    Exp* l = parseAnd();
    while (match(Token::OR)) {
        Exp* r = parseAnd();
        l = new BinaryExp(l, r, OR_OP);
    }
    return l;
}

// And ::= Eq { '&&' Eq }
Exp* Parser::parseAnd() {
    Exp* l = parseEq();
    while (match(Token::AND)) {
        Exp* r = parseEq();
        l = new BinaryExp(l, r, AND_OP);
    }
    return l;
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
        BinaryOp op = previous->type == Token::PLUS ? PLUS_OP : MINUS_OP;
        Exp* r = parseExpr();
        l = new BinaryExp(l, r, op);
    }
    return l;
}

// expr ::= term { (* | / | %) term }*
Exp* Parser::parseExpr() {
    Exp* l = parseTerm();
    while (match(Token::MUL) || match(Token::DIV) || match(Token::MOD)) {
        BinaryOp op = previous->type == Token::MUL ? MUL_OP
                    : previous->type == Token::DIV ? DIV_OP : MOD_OP;
        Exp* r = parseTerm();
        l = new BinaryExp(l, r, op);
    }
    return l;
}

// term ::= '-' term | '!' term | ('++' | '--') term | factor
Exp* Parser::parseTerm() {
    if (match(Token::MINUS)) {
        return new UnaryExp(parseTerm(), NEG_OP);
    }
    if (match(Token::NOT)) {
        return new UnaryExp(parseTerm(), NOT_OP);
    }
    if (match(Token::INC) || match(Token::DEC)) {
        bool incremento = previous->type == Token::INC;
        Exp* e = parseTerm();
        if (IdExp* id = dynamic_cast<IdExp*>(e)) {
            string nombre = id->value;
            delete id;
            return new IncDecExp(nombre, {}, incremento, true);
        }
        if (ArrayExp* a = dynamic_cast<ArrayExp*>(e)) {
            IncDecExp* r = new IncDecExp(a->nombre, a->indices, incremento, true);
            a->indices.clear();
            delete a;
            return r;
        }
        throw runtime_error(string("Error sintáctico: '") + (incremento ? "++" : "--")
                            + "' necesita una variable o un elemento de arreglo");
    }
    return parseFactor();
}

// factor ::= number | true | false | char | string+ | '(' CExp ')'
//          | id '(' [CExp (',' CExp)*] ')'
//          | id { '[' CExp ']' } [ '++' | '--' ]
Exp* Parser::parseFactor() {
    Exp* objeto = parseAtomo();
    while (match(Token::DOT)) {
        objeto = new CampoExp(objeto, consumirNombre("se esperaba el nombre del campo"));
    }
    return objeto;
}

Exp* Parser::parseAtomo() {
    if (match(Token::NUM)) {
        long long v;
        try {
            v = stoll(previous->text);
        } catch (const exception&) {
            throw runtime_error("Error sintáctico: literal entero demasiado grande: " + previous->text);
        }
        // En C++ un literal decimal es int si cabe en int; si no, long
        if (v <= 2147483647LL) return new NumberExp(v);
        return NumberExp::crearLong(v);
    }
    else if (match(Token::FLOATNUM)) {
        return NumberExp::crearDouble(stod(previous->text));
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
        // Literales consecutivos se concatenan: "ab" "cd" == "abcd"
        string valor;
        do {
            string crudo = previous->text;
            valor += desescapar(crudo.substr(1, crudo.size() - 2));
        } while (match(Token::STRLIT));
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
        if (tiposEstructura.count(nombre) && check(Token::LBRACE)) {
            return parseInicializadorStruct(nombre);
        }
        if (match(Token::LPAREN)) {                 // llamada: id '(' [CExp (',' CExp)*] ')'
            FCallExp* call = new FCallExp();
            call->nombre = nombre;
            call->usingStd = usingStd;
            if (nombre.rfind("std::", 0) == 0 && includes.empty()) {
                throw runtime_error("Error sintáctico: '" + nombre + "' no está declarado (falta #include)");
            }
            if (!check(Token::RPAREN)) {
                do {
                    call->argumentos.push_back(parseCExp());
                } while (match(Token::COMMA));
            }
            consume(Token::RPAREN, "se esperaba ')' en la llamada a " + nombre);
            return call;
        }
        vector<Exp*> indices;                       // acceso a arreglo: id[i][j]
        while (match(Token::LBRACKET)) {
            indices.push_back(parseCExp());
            consume(Token::RBRACKET, "se esperaba ']'");
        }
        if (match(Token::INC) || match(Token::DEC)) {   // x++ / a[i]--
            return new IncDecExp(nombre, indices, previous->type == Token::INC, false);
        }
        if (!indices.empty()) return new ArrayExp(nombre, indices);
        return new IdExp(nombre);
    }
    else if (check(Token::RESERVED) || check(Token::TVOID)) {
        throw runtime_error("Error sintáctico: '" + current->text + "' no está soportado en este subconjunto de C++");
    }
    else {
        string encontrado = isAtEnd() ? "fin de archivo" : "'" + current->text + "'";
        throw runtime_error("Error sintáctico: se esperaba una expresión (se encontró " + encontrado + ")");
    }
}
