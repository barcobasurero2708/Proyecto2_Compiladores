#include <iostream>
#include "token.h"

using namespace std;

// Crea un token sin texto (por ejemplo, END)
Token::Token(Type type)
    : type(type), text("") { }

// Crea un token cuyo texto es un solo carácter
Token::Token(Type type, char c)
    : type(type), text(string(1, c)) { }

// Crea un token con el texto source[first, first + last)
Token::Token(Type type, const string& source, int first, int last)
    : type(type), text(source.substr(first, last)) { }

// Imprime el token con el formato TOKEN(TIPO, "texto")
ostream& operator<<(ostream& outs, const Token& tok) {
    switch (tok.type) {
        case Token::PLUS:     outs << "TOKEN(PLUS, \""     << tok.text << "\")"; break;
        case Token::MINUS:    outs << "TOKEN(MINUS, \""    << tok.text << "\")"; break;
        case Token::MUL:      outs << "TOKEN(MUL, \""      << tok.text << "\")"; break;
        case Token::DIV:      outs << "TOKEN(DIV, \""      << tok.text << "\")"; break;
        case Token::POW:      outs << "TOKEN(POW, \""      << tok.text << "\")"; break;
        case Token::LPAREN:   outs << "TOKEN(LPAREN, \""   << tok.text << "\")"; break;
        case Token::RPAREN:   outs << "TOKEN(RPAREN, \""   << tok.text << "\")"; break;
        case Token::NUM:      outs << "TOKEN(NUM, \""      << tok.text << "\")"; break;
        case Token::ERR:      outs << "TOKEN(ERR, \""      << tok.text << "\")"; break;
        case Token::ID:       outs << "TOKEN(ID, \""       << tok.text << "\")"; break;
        case Token::LE:       outs << "TOKEN(LE, \""       << tok.text << "\")"; break;
        case Token::SEMICOL:  outs << "TOKEN(SEMICOL, \""  << tok.text << "\")"; break;
        case Token::ASSIGN:   outs << "TOKEN(ASSIGN, \""   << tok.text << "\")"; break;
        case Token::IF:       outs << "TOKEN(IF, \""       << tok.text << "\")"; break;
        case Token::WHILE:    outs << "TOKEN(WHILE, \""    << tok.text << "\")"; break;
        case Token::DO:       outs << "TOKEN(DO, \""       << tok.text << "\")"; break;
        case Token::ELSE:    outs << "TOKEN(ELSE, \""     << tok.text << "\")"; break;
        case Token::BREAK:    outs << "TOKEN(BREAK, \""    << tok.text << "\")"; break;
        case Token::IFEXP:    outs << "TOKEN(IFEXP, \""    << tok.text << "\")"; break;
        case Token::COMMA:    outs << "TOKEN(COMMA, \""    << tok.text << "\")"; break;
        case Token::TRUE:     outs << "TOKEN(TRUE, \""     << tok.text << "\")"; break;
        case Token::FALSE:    outs << "TOKEN(FALSE, \""    << tok.text << "\")"; break;
        case Token::RETURN:   outs << "TOKEN(RETURN, \""   << tok.text << "\")"; break;
        case Token::LBRACE:    outs << "TOKEN(LBRACE, \""    << tok.text << "\")"; break;
        case Token::RBRACE:    outs << "TOKEN(RBRACE, \""    << tok.text << "\")"; break;
        case Token::SHL:       outs << "TOKEN(SHL, \""       << tok.text << "\")"; break;
        case Token::TINT:      outs << "TOKEN(TINT, \""      << tok.text << "\")"; break;
        case Token::TBOOL:     outs << "TOKEN(TBOOL, \""     << tok.text << "\")"; break;
        case Token::COUT:      outs << "TOKEN(COUT, \""      << tok.text << "\")"; break;
        case Token::ENDL:      outs << "TOKEN(ENDL, \""      << tok.text << "\")"; break;
        case Token::USING:     outs << "TOKEN(USING, \""     << tok.text << "\")"; break;
        case Token::NAMESPACE: outs << "TOKEN(NAMESPACE, \"" << tok.text << "\")"; break;
        case Token::INCLUDE:   outs << "TOKEN(INCLUDE, \"" << tok.text << "\")"; break;
        case Token::QUESTION:  outs << "TOKEN(QUESTION, \"" << tok.text << "\")"; break;
        case Token::COLON:     outs << "TOKEN(COLON, \""    << tok.text << "\")"; break;
        case Token::LT:        outs << "TOKEN(LT, \""       << tok.text << "\")"; break;
        case Token::GT:        outs << "TOKEN(GT, \""       << tok.text << "\")"; break;
        case Token::GE:        outs << "TOKEN(GE, \""       << tok.text << "\")"; break;
        case Token::EQ:        outs << "TOKEN(EQ, \""       << tok.text << "\")"; break;
        case Token::NE:        outs << "TOKEN(NE, \""       << tok.text << "\")"; break;
        case Token::FLOATNUM:  outs << "TOKEN(FLOATNUM, \"" << tok.text << "\")"; break;
        case Token::CHARLIT:   outs << "TOKEN(CHARLIT, \""  << tok.text << "\")"; break;
        case Token::STRLIT:    outs << "TOKEN(STRLIT, \""   << tok.text << "\")"; break;
        case Token::TCHAR:     outs << "TOKEN(TCHAR, \""    << tok.text << "\")"; break;
        case Token::TFLOAT:    outs << "TOKEN(TFLOAT, \""   << tok.text << "\")"; break;
        case Token::TSTRING:   outs << "TOKEN(TSTRING, \""  << tok.text << "\")"; break;
        case Token::TDOUBLE:   outs << "TOKEN(TDOUBLE, \""  << tok.text << "\")"; break;
        case Token::TLONG:     outs << "TOKEN(TLONG, \""    << tok.text << "\")"; break;
        case Token::TVOID:     outs << "TOKEN(TVOID, \""    << tok.text << "\")"; break;
        case Token::RESERVED:  outs << "TOKEN(RESERVED, \"" << tok.text << "\")"; break;
        case Token::MOD:       outs << "TOKEN(MOD, \"" << tok.text << "\")"; break;
        case Token::AND:       outs << "TOKEN(AND, \"" << tok.text << "\")"; break;
        case Token::OR:        outs << "TOKEN(OR, \"" << tok.text << "\")"; break;
        case Token::NOT:       outs << "TOKEN(NOT, \"" << tok.text << "\")"; break;
        case Token::INC:       outs << "TOKEN(INC, \"" << tok.text << "\")"; break;
        case Token::DEC:       outs << "TOKEN(DEC, \"" << tok.text << "\")"; break;
        case Token::FOR:       outs << "TOKEN(FOR, \"" << tok.text << "\")"; break;
        case Token::CONTINUE:  outs << "TOKEN(CONTINUE, \"" << tok.text << "\")"; break;
        case Token::LBRACKET:  outs << "TOKEN(LBRACKET, \"" << tok.text << "\")"; break;
        case Token::RBRACKET:  outs << "TOKEN(RBRACKET, \"" << tok.text << "\")"; break;
        case Token::STRUCT:   outs << "TOKEN(STRUCT, struct)"; break;
        case Token::OPERATOR: outs << "TOKEN(OPERATOR, operator)"; break;
        case Token::DOT:      outs << "TOKEN(DOT, .)"; break;
        case Token::TEMPLATE: outs << "TOKEN(TEMPLATE, template)"; break;
        case Token::TYPENAME: outs << "TOKEN(TYPENAME, typename)"; break;
        case Token::END:      outs << "TOKEN(END)"; break;
    }
    return outs;
}

// Imprime un puntero a token (o TOKEN(NULL) si es nulo)
ostream& operator<<(ostream& outs, const Token* tok) {
    if (!tok) return outs << "TOKEN(NULL)";
    return outs << *tok;
}
