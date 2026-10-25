#include <iostream>
#include <cstring>
#include <fstream>
#include "token.h"
#include "scanner.h"

using namespace std;

// Indica si el carácter es un espacio en blanco
bool is_white_space(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

// Inicializa el scanner al inicio de la entrada
Scanner::Scanner(const char* s) : input(s), first(0), current(0) { }

// Reconoce y devuelve el siguiente token de la entrada
Token* Scanner::nextToken() {
    Token* token;

    while (current < input.length() && is_white_space(input[current]))
        current++;

    if (current >= input.length())
        return new Token(Token::END);

    char c = input[current];
    first = current;

    if (isdigit(c)) {
        current++;
        while (current < input.length() && isdigit(input[current]))
            current++;
        token = new Token(Token::NUM, input, first, current - first);
    }
    else if (isalpha(c) || c == '_') {
        current++;
        while (current < input.length() && (isalnum(input[current]) || input[current] == '_'))
            current++;
        string lexema = input.substr(first, current - first);
        if (lexema == "sqrt")          return new Token(Token::SQRT,     input, first, current - first);
        else if (lexema == "print")    return new Token(Token::PRINT,    input, first, current - first);
        else if (lexema == "if")       return new Token(Token::IF,       input, first, current - first);
        else if (lexema == "while")    return new Token(Token::WHILE,    input, first, current - first);
        else if (lexema == "then")     return new Token(Token::THEN,     input, first, current - first);
        else if (lexema == "do")       return new Token(Token::DO,       input, first, current - first);
        else if (lexema == "endif")    return new Token(Token::ENDIF,    input, first, current - first);
        else if (lexema == "endwhile") return new Token(Token::ENDWHILE, input, first, current - first);
        else if (lexema == "enddo")    return new Token(Token::ENDDO,    input, first, current - first);
        else if (lexema == "else")    return new Token(Token::ELSE,     input, first, current - first);
        else if (lexema == "break")    return new Token(Token::BREAK,    input, first, current - first);
        else if (lexema == "ifexp")    return new Token(Token::IFEXP,    input, first, current - first);
        else if (lexema == "true")     return new Token(Token::TRUE,     input, first, current - first);
        else if (lexema == "false")    return new Token(Token::FALSE,    input, first, current - first);
        else if (lexema == "var")      return new Token(Token::VAR,      input, first, current - first);
        else if (lexema == "fun")      return new Token(Token::FUN,      input, first, current - first);
        else if (lexema == "endfun")   return new Token(Token::ENDFUN,   input, first, current - first);
        else if (lexema == "return")   return new Token(Token::RETURN,   input, first, current - first);
        else if (lexema == "max")      return new Token(Token::MAX,      input, first, current - first);
        else if (lexema == "min")      return new Token(Token::MIN,      input, first, current - first);
        else if (lexema == "count")    return new Token(Token::COUNT,    input, first, current - first);
        else                           return new Token(Token::ID,       input, first, current - first);
    }
    else if (strchr("+/-*();=<,", c)) {
        switch (c) {
            case '+': token = new Token(Token::PLUS,    c); break;
            case '-': token = new Token(Token::MINUS,   c); break;
            case '/': token = new Token(Token::DIV,     c); break;
            case '(': token = new Token(Token::LPAREN,  c); break;
            case ')': token = new Token(Token::RPAREN,  c); break;
            case '=': token = new Token(Token::ASSIGN,  c); break;
            case ';': token = new Token(Token::SEMICOL, c); break;
            case ',': token = new Token(Token::COMMA,   c); break;
            case '*':
                if (input[current + 1] == '*') {
                    current++;
                    token = new Token(Token::POW, input, first, current + 1 - first);
                }
                else {
                    token = new Token(Token::MUL, c);
                }
                break;
            case '<':
                if (input[current + 1] == '=') {
                    current++;
                    token = new Token(Token::LE, input, first, current + 1 - first);
                }
                else {
                    token = new Token(Token::ERR, c);
                }
                break;
        }
        current++;
    }
    else {
        token = new Token(Token::ERR, c);
        current++;
    }

    return token;
}

// Destructor del scanner
Scanner::~Scanner() { }

// Escanea toda la entrada y escribe los tokens en <archivo>_tokens.txt
void ejecutar_scanner(Scanner* scanner, const string& InputFile) {
    Token* tok;

    string OutputFileName = InputFile;
    size_t pos = OutputFileName.find_last_of(".");
    if (pos != string::npos) {
        OutputFileName = OutputFileName.substr(0, pos);
    }
    OutputFileName += "_tokens.txt";

    ofstream outFile(OutputFileName);
    if (!outFile.is_open()) {
        cerr << "Error: no se pudo abrir el archivo " << OutputFileName << endl;
        return;
    }

    outFile << "Scanner\n" << endl;

    while (true) {
        tok = scanner->nextToken();

        if (tok->type == Token::END) {
            outFile << *tok << endl;
            delete tok;
            outFile << "\nScanner exitoso" << endl << endl;
            outFile.close();
            return;
        }

        if (tok->type == Token::ERR) {
            outFile << *tok << endl;
            delete tok;
            outFile << "Caracter invalido" << endl << endl;
            outFile << "Scanner no exitoso" << endl << endl;
            outFile.close();
            return;
        }

        outFile << *tok << endl;
        delete tok;
    }
}
