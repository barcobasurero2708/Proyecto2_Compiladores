#include <iostream>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_map>
#include <cstdio>
#include <cstdint>
#include <climits>
#include <map>
#include <cstring>
#include "ast.h"
#include "parser.h"
#include "visitor.h"

using namespace std;


// =====================================================
// ====================== accept =======================
// =====================================================

Type BinaryExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type StructExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type CampoExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type UnaryExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type ArrayExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type IncDecExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type DireccionExp::accept(Visitor* visitor) { return visitor->visit(this); }
Type DerefExp::accept(Visitor* visitor)   { return visitor->visit(this); }
Type LongitudExp::accept(Visitor* visitor) { return visitor->visit(this); }
Type NumberExp::accept(Visitor* visitor)  { return visitor->visit(this); }
Type IdExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type IfExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type FCallExp::accept(Visitor* visitor)   { return visitor->visit(this); }

void AssignStmt::accept(Visitor* visitor)  { visitor->visit(this); }
void AssignPunteroStmt::accept(Visitor* visitor) { visitor->visit(this); }
void PrintStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void IfStmt::accept(Visitor* visitor)      { visitor->visit(this); }
void WhileStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void DoWhileStmt::accept(Visitor* visitor) { visitor->visit(this); }
void BreakStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void ReturnStmt::accept(Visitor* visitor)  { visitor->visit(this); }
void BlockStmt::accept(Visitor* visitor)   { visitor->visit(this); }
void ExprStmt::accept(Visitor* visitor)    { visitor->visit(this); }
void ForStmt::accept(Visitor* visitor)     { visitor->visit(this); }
void ContinueStmt::accept(Visitor* visitor) { visitor->visit(this); }
void SwitchStmt::accept(Visitor* visitor)  { visitor->visit(this); }

void VarDec::accept(Visitor* visitor)   { visitor->visit(this); }
void Body::accept(Visitor* visitor)     { visitor->visit(this); }
void FunDec::accept(Visitor* visitor)   { visitor->visit(this); }
void Programa::accept(Visitor* visitor) { visitor->visit(this); }

