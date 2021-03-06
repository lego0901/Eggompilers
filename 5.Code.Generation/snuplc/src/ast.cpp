//------------------------------------------------------------------------------
/// @brief SnuPL abstract syntax tree
/// @author Bernhard Egger <bernhard@csap.snu.ac.kr>
/// @section changelog Change Log
/// 2012/09/14 Bernhard Egger created
/// 2013/05/22 Bernhard Egger reimplemented TAC generation
/// 2013/11/04 Bernhard Egger added typechecks for unary '+' operators
/// 2016/03/12 Bernhard Egger adapted to SnuPL/1
/// 2014/04/08 Bernhard Egger assignment 2: AST for SnuPL/-1
///
/// @section license_section License
/// Copyright (c) 2012-2016 Bernhard Egger
/// All rights reserved.
///
/// Redistribution and use in source and binary forms,  with or without modifi-
/// cation, are permitted provided that the following conditions are met:
///
/// - Redistributions of source code must retain the above copyright notice,
///   this list of conditions and the following disclaimer.
/// - Redistributions in binary form must reproduce the above copyright notice,
///   this list of conditions and the following disclaimer in the documentation
///   and/or other materials provided with the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
/// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
/// IMPLIED WARRANTIES OF MERCHANTABILITY  AND FITNESS FOR A PARTICULAR PURPOSE
/// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER  OR CONTRIBUTORS BE
/// LIABLE FOR ANY DIRECT,  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSE-
/// QUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF  SUBSTITUTE
/// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
/// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT
/// LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY
/// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
/// DAMAGE.
//------------------------------------------------------------------------------

#include <iostream>
#include <cassert>
#include <cstring>

#include <typeinfo>

#include "ast.h"
using namespace std;


//------------------------------------------------------------------------------
// CAstNode
//
int CAstNode::_global_id = 0;

CAstNode::CAstNode(CToken token)
  : _token(token), _addr(NULL)
{
  _id = _global_id++;
}

CAstNode::~CAstNode(void)
{
  if (_addr != NULL) delete _addr;
}

int CAstNode::GetID(void) const
{
  return _id;
}

CToken CAstNode::GetToken(void) const
{
  return _token;
}

const CType* CAstNode::GetType(void) const
{
  return CTypeManager::Get()->GetNull();
}

string CAstNode::dotID(void) const
{
  ostringstream out;
  out << "node" << dec << _id;
  return out.str();
}

string CAstNode::dotAttr(void) const
{
  return " [label=\"" + dotID() + "\"]";
}

void CAstNode::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << dotID() << dotAttr() << ";" << endl;
}

CTacAddr* CAstNode::GetTacAddr(void) const
{
  return _addr;
}

ostream& operator<<(ostream &out, const CAstNode &t)
{
  return t.print(out);
}

ostream& operator<<(ostream &out, const CAstNode *t)
{
  return t->print(out);
}

//------------------------------------------------------------------------------
// CAstScope
//
CAstScope::CAstScope(CToken t, const string name, CAstScope *parent)
  : CAstNode(t), _name(name), _symtab(NULL), _parent(parent), _statseq(NULL),
    _cb(NULL)
{
  if (_parent != NULL) _parent->AddChild(this);
}

CAstScope::~CAstScope(void)
{
  delete _symtab;
  delete _statseq;
  delete _cb;
}

const string CAstScope::GetName(void) const
{
  return _name;
}

CAstScope* CAstScope::GetParent(void) const
{
  return _parent;
}

size_t CAstScope::GetNumChildren(void) const
{
  return _children.size();
}

CAstScope* CAstScope::GetChild(size_t i) const
{
  assert(i < _children.size());
  return _children[i];
}

CSymtab* CAstScope::GetSymbolTable(void) const
{
  assert(_symtab != NULL);
  return _symtab;
}

void CAstScope::SetStatementSequence(CAstStatement *statseq)
{
  _statseq = statseq;
}

CAstStatement* CAstScope::GetStatementSequence(void) const
{
  return _statseq;
}

bool CAstScope::TypeCheck(CToken *t, string *msg) const
{
  bool result = true;

  try {
    // Typecheck for all statements
    CAstStatement *st = _statseq;
    while (result && st) {
      result = st->TypeCheck(t, msg);
      st = st->GetNext();
    }

    // Typecheck for all child scopes
    size_t size = GetNumChildren();
    for (size_t i = 0; i < size && result; i++) {
      CAstScope *child = GetChild(i);
      result = child->TypeCheck(t, msg);
    }
  }
  catch (...) {
    result = false;
  }

  return result;
}

ostream& CAstScope::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "CAstScope: '" << _name << "'" << endl;
  out << ind << "  symbol table:" << endl;
  _symtab->print(out, indent+4);
  out << ind << "  statement list:" << endl;
  CAstStatement *s = GetStatementSequence();
  if (s != NULL) {
    do {
      s->print(out, indent+4);
      s = s->GetNext();
    } while (s != NULL);
  } else {
    out << ind << "    empty." << endl;
  }

  out << ind << "  nested scopes:" << endl;
  if (_children.size() > 0) {
    for (size_t i=0; i<_children.size(); i++) {
      _children[i]->print(out, indent+4);
    }
  } else {
    out << ind << "    empty." << endl;
  }
  out << ind << endl;

  return out;
}

void CAstScope::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  CAstStatement *s = GetStatementSequence();
  if (s != NULL) {
    string prev = dotID();
    do {
      s->toDot(out, indent);
      out << ind << prev << " -> " << s->dotID() << " [style=dotted];" << endl;
      prev = s->dotID();
      s = s->GetNext();
    } while (s != NULL);
  }

  vector<CAstScope*>::const_iterator it = _children.begin();
  while (it != _children.end()) {
    CAstScope *s = *it++;
    s->toDot(out, indent);
    out << ind << dotID() << " -> " << s->dotID() << ";" << endl;
  }

}

CTacAddr* CAstScope::ToTac(CCodeBlock *cb)
{
  assert(cb);

  CAstStatement *s = GetStatementSequence();
  while (s) {
    CTacLabel *next = cb->CreateLabel();
    s->ToTac(cb, next);
    cb->AddInstr(next);
    s = s->GetNext();
  }

  // removes redundant labels and goto instructions.
  cb->CleanupControlFlow();

  return NULL;
}

CCodeBlock* CAstScope::GetCodeBlock(void) const
{
  return _cb;
}

void CAstScope::SetSymbolTable(CSymtab *st)
{
  if (_symtab != NULL) delete _symtab;
  _symtab = st;
}

void CAstScope::AddChild(CAstScope *child)
{
  _children.push_back(child);
}


//------------------------------------------------------------------------------
// CAstModule
//
CAstModule::CAstModule(CToken t, const string name)
  : CAstScope(t, name, NULL)
{
  SetSymbolTable(new CSymtab());
}

CSymbol* CAstModule::CreateVar(const string ident, const CType *type)
{
  return new CSymGlobal(ident, type);
}

string CAstModule::dotAttr(void) const
{
  return " [label=\"m " + GetName() + "\",shape=box]";
}



//------------------------------------------------------------------------------
// CAstProcedure
//
CAstProcedure::CAstProcedure(CToken t, const string name,
                             CAstScope *parent, CSymProc *symbol)
  : CAstScope(t, name, parent), _symbol(symbol)
{
  assert(GetParent() != NULL);
  SetSymbolTable(new CSymtab(GetParent()->GetSymbolTable()));
  assert(_symbol != NULL);
}

CSymProc* CAstProcedure::GetSymbol(void) const
{
  return _symbol;
}

CSymbol* CAstProcedure::CreateVar(const string ident, const CType *type)
{
  return new CSymLocal(ident, type);
}

const CType* CAstProcedure::GetType(void) const
{
  return GetSymbol()->GetDataType();
}

string CAstProcedure::dotAttr(void) const
{
  return " [label=\"p/f " + GetName() + "\",shape=box]";
}


//------------------------------------------------------------------------------
// CAstType
//
CAstType::CAstType(CToken t, const CType *type)
  : CAstNode(t), _type(type)
{
  assert(type != NULL);
}

const CType* CAstType::GetType(void) const
{
  return _type;
}

ostream& CAstType::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "CAstType (" << _type << ")" << endl;
  return out;
}


//------------------------------------------------------------------------------
// CAstStatement
//
CAstStatement::CAstStatement(CToken token)
  : CAstNode(token), _next(NULL)
{
}

CAstStatement::~CAstStatement(void)
{
  delete _next;
}

void CAstStatement::SetNext(CAstStatement *next)
{
  _next = next;
}

CAstStatement* CAstStatement::GetNext(void) const
{
  return _next;
}


//------------------------------------------------------------------------------
// CAstStatAssign
//
CAstStatAssign::CAstStatAssign(CToken t,
                               CAstDesignator *lhs, CAstExpression *rhs)
  : CAstStatement(t), _lhs(lhs), _rhs(rhs)
{
  assert(lhs != NULL);
  assert(rhs != NULL);
}

CAstDesignator* CAstStatAssign::GetLHS(void) const
{
  return _lhs;
}

CAstExpression* CAstStatAssign::GetRHS(void) const
{
  return _rhs;
}

bool CAstStatAssign::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;
  CAstDesignator *lhs = GetLHS();
  CAstExpression *rhs = GetRHS();

  if (!lhs->TypeCheck(t, msg))
    return false;

  if (!rhs->TypeCheck(t, msg))
    return false;

  // the type of lhs should not INVALID or non-scalar type
  if (!lhs->GetType() || !lhs->GetType()->IsScalar()) {
    if (t) *t = lhs->GetToken();
    if (msg) {
      out << "invalid variable type." << endl;
      // out << "\"" << lhs->GetSymbol()->GetName() << "\" : ";
      out << "LHS : ";
      if (lhs->GetType()) out << lhs->GetType() << endl;
      else out << "<INVALID>" << endl;
      *msg = out.str();
    }
    return false;
  }

  // the type of rhs should not INVALID or non-scalar type
  if (!rhs->GetType() || !rhs->GetType()->IsScalar()) {
    if (t) *t = rhs->GetToken();
    if (msg) {
      out << "invalid value type." << endl;
      // out << "\"" << rhs->GetSymbol()->GetName() << "\" : ";
      out << "RHS : ";
      if (rhs->GetType()) out << rhs->GetType() << endl;
      else out << "<INVALID>" << endl;
      *msg = out.str();
    }
    return false;
  }

  // the type of lhs matches with the type of rhs
  if (!lhs->GetType()->Match(rhs->GetType())) {
    if (t) *t = lhs->GetToken();
    if (msg) {
      out << "assign type mismatch." << endl;
      out << "LHS : " << lhs->GetType() << endl;
      out << "RHS : " << rhs->GetType() << endl;
      *msg = out.str();
    }
    return false;
  }

  return true;
}

const CType* CAstStatAssign::GetType(void) const
{
  return _lhs->GetType();
}

ostream& CAstStatAssign::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << ":=" << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  _lhs->print(out, indent+2);
  _rhs->print(out, indent+2);

  return out;
}

string CAstStatAssign::dotAttr(void) const
{
  return " [label=\":=\",shape=box]";
}

void CAstStatAssign::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _lhs->toDot(out, indent);
  out << ind << dotID() << "->" << _lhs->dotID() << ";" << endl;
  _rhs->toDot(out, indent);
  out << ind << dotID() << "->" << _rhs->dotID() << ";" << endl;
}

CTacAddr* CAstStatAssign::ToTac(CCodeBlock *cb, CTacLabel *next)
{
  // CTacAddr *dst = GetLHS()->ToTac(cb);
  CTacAddr *src = GetRHS()->ToTac(cb); // gets the TAC of RHS
  CTacAddr *dst = GetLHS()->ToTac(cb); // gets the TAC of LHS

  cb->AddInstr(new CTacInstr(opAssign, dst, src));
  cb->AddInstr(new CTacInstr(opGoto, next));

  return NULL;
}


//------------------------------------------------------------------------------
// CAstStatCall
//
CAstStatCall::CAstStatCall(CToken t, CAstFunctionCall *call)
  : CAstStatement(t), _call(call)
{
  assert(call != NULL);
}

CAstFunctionCall* CAstStatCall::GetCall(void) const
{
  return _call;
}

bool CAstStatCall::TypeCheck(CToken *t, string *msg) const
{
  return GetCall()->TypeCheck(t, msg);
}

ostream& CAstStatCall::print(ostream &out, int indent) const
{
  _call->print(out, indent);

  return out;
}

string CAstStatCall::dotID(void) const
{
  return _call->dotID();
}

string CAstStatCall::dotAttr(void) const
{
  return _call->dotAttr();
}

void CAstStatCall::toDot(ostream &out, int indent) const
{
  _call->toDot(out, indent);
}

CTacAddr* CAstStatCall::ToTac(CCodeBlock *cb, CTacLabel *next)
{
  CAstFunctionCall *call = GetCall();
  CTacTemp *tmp = NULL;
  int n = call->GetNArgs();

  // build param instructions by getting the TAC of each argument
  for (int i = n - 1; i >= 0; i--) {
    CTacAddr *argTac = call->GetArg(i)->ToTac(cb);
    CTacInstr *instr = new CTacInstr(opParam, new CTacConst(i), argTac, NULL);
    cb->AddInstr(instr);
  }

 // Add call instruction
  if (call->GetType() != CTypeManager::Get()->GetNull())
    tmp = cb->CreateTemp(call->GetType());
  cb->AddInstr(new CTacInstr(opCall, tmp, new CTacName(call->GetSymbol()), NULL));

  cb->AddInstr(new CTacInstr(opGoto, next, NULL, NULL));

  return NULL;
}


//------------------------------------------------------------------------------
// CAstStatReturn
//
CAstStatReturn::CAstStatReturn(CToken t, CAstScope *scope, CAstExpression *expr)
  : CAstStatement(t), _scope(scope), _expr(expr)
{
  assert(scope != NULL);
}

CAstScope* CAstStatReturn::GetScope(void) const
{
  return _scope;
}

CAstExpression* CAstStatReturn::GetExpression(void) const
{
  return _expr;
}

bool CAstStatReturn::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;
  const CType *st = GetScope()->GetType();
  CAstExpression *e = GetExpression();

  // if procedure has "return expression"
  if (st->Match(CTypeManager::Get()->GetNull())) {
    if (e) {
      if (t) *t = e->GetToken();
      if (msg) {
        out << "procedure should have no return value/expression." << endl;
        *msg = out.str();
      }
      return false;
    }
  }
  else { // if function has "return" and no expression right after "return"
    if (!e) {
      if (t) *t = GetToken();
      if (msg) {
        out << "function should have return value/expression." << endl;
        *msg = out.str();
      }
      return false;
    }

    if (!e->TypeCheck(t, msg))
      return false;

    // if return type mismatched
    if (!st->Match(e->GetType())) {
      if (t) *t = e->GetToken();
      if (msg) {
        out << "return type mismatch." << endl;
        out << st << " type expected, but it returns ";
        if (e->GetType() == NULL) out << "<INVALID>" << endl;
        else out << e->GetType() << endl;
        *msg = out.str();
      }
      return false;
    }
  }

  return true;
}

const CType* CAstStatReturn::GetType(void) const
{
  const CType *t = NULL;

  if (GetExpression() != NULL)
    t = GetExpression()->GetType();
  else
    t = CTypeManager::Get()->GetNull();

  return t;
}

ostream& CAstStatReturn::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "return" << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  if (_expr != NULL) _expr->print(out, indent+2);

  return out;
}

string CAstStatReturn::dotAttr(void) const
{
  return " [label=\"return\",shape=box]";
}

void CAstStatReturn::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  if (_expr != NULL) {
    _expr->toDot(out, indent);
    out << ind << dotID() << "->" << _expr->dotID() << ";" << endl;
  }
}

CTacAddr* CAstStatReturn::ToTac(CCodeBlock *cb, CTacLabel *next)
{
  CTacAddr *retval = NULL;

  // if expression exists, set retval to the TAC of the expression
  if (GetExpression())
    retval = GetExpression()->ToTac(cb);

  cb->AddInstr(new CTacInstr(opReturn, NULL, retval, NULL));
  cb->AddInstr(new CTacInstr(opGoto, next, NULL, NULL));

  return NULL;
}


//------------------------------------------------------------------------------
// CAstStatIf
//
CAstStatIf::CAstStatIf(CToken t, CAstExpression *cond,
                       CAstStatement *ifBody, CAstStatement *elseBody)
  : CAstStatement(t), _cond(cond), _ifBody(ifBody), _elseBody(elseBody)
{
  assert(cond != NULL);
}

CAstExpression* CAstStatIf::GetCondition(void) const
{
  return _cond;
}

CAstStatement* CAstStatIf::GetIfBody(void) const
{
  return _ifBody;
}

CAstStatement* CAstStatIf::GetElseBody(void) const
{
  return _elseBody;
}

bool CAstStatIf::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;
  CAstExpression *cond = GetCondition();
  CAstStatement *ifBody = GetIfBody();
  CAstStatement *elseBody = GetElseBody();

  // Type check for condition expression
  if (!cond->TypeCheck(t, msg))
    return false;

  // check whether its type is boolean type
  if (!cond->GetType() || !cond->GetType()->Match(CTypeManager::Get()->GetBool())) {
    if (t) *t = cond->GetToken();
    if (msg) {
      out << "condition should be bool type, but ";
      if (cond->GetType()) out << cond->GetType();
      else out << "<INVALID>";
      out << " appeared" << endl;
      *msg = out.str();
    }
    return false;
  }

  // Type check for statements in ifBody (can be empty)
  while (ifBody) {
    if (!ifBody->TypeCheck(t, msg))
      return false;
    ifBody = ifBody->GetNext();
  }

  // Type check for statements in elseBody (can be empty)
  while (elseBody) {
    if (!elseBody->TypeCheck(t, msg))
      return false;
    elseBody = elseBody->GetNext();
  }

  return true;
}

ostream& CAstStatIf::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "if cond" << endl;
  _cond->print(out, indent+2);
  out << ind << "if-body" << endl;
  if (_ifBody != NULL) {
    CAstStatement *s = _ifBody;
    do {
      s->print(out, indent+2);
      s = s->GetNext();
    } while (s != NULL);
  } else out << ind << "  empty." << endl;
  out << ind << "else-body" << endl;
  if (_elseBody != NULL) {
    CAstStatement *s = _elseBody;
    do {
      s->print(out, indent+2);
      s = s->GetNext();
    } while (s != NULL);
  } else out << ind << "  empty." << endl;

  return out;
}

string CAstStatIf::dotAttr(void) const
{
  return " [label=\"if\",shape=box]";
}

void CAstStatIf::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _cond->toDot(out, indent);
  out << ind << dotID() << "->" << _cond->dotID() << ";" << endl;

  if (_ifBody != NULL) {
    CAstStatement *s = _ifBody;
    if (s != NULL) {
      string prev = dotID();
      do {
        s->toDot(out, indent);
        out << ind << prev << " -> " << s->dotID() << " [style=dotted];"
            << endl;
        prev = s->dotID();
        s = s->GetNext();
      } while (s != NULL);
    }
  }

  if (_elseBody != NULL) {
    CAstStatement *s = _elseBody;
    if (s != NULL) {
      string prev = dotID();
      do {
        s->toDot(out, indent);
        out << ind << prev << " -> " << s->dotID() << " [style=dotted];"
            << endl;
        prev = s->dotID();
        s = s->GetNext();
      } while (s != NULL);
    }
  }
}

CTacAddr* CAstStatIf::ToTac(CCodeBlock *cb, CTacLabel *next)
{
  CTacLabel *nextTrue = cb->CreateLabel("if_true");
  CTacLabel *nextFalse = cb->CreateLabel("if_false");
  CAstStatement *ifBody = GetIfBody(), *elseBody = GetElseBody();

  /* The TAC of CAstStatIf has the form as follwing;
   *
   * if (the condition is true) goto if_true
   * goto if_false
   * if_true:
   *   (ifBody statement sequence)
   *   goto next
   * if_false:
   *   (elseBody statement sequence)
   * next:
   */

  GetCondition()->ToTac(cb, nextTrue, nextFalse);

  cb->AddInstr(nextTrue);
  while (ifBody) {
    CTacLabel *nextIfBody = cb->CreateLabel();
    ifBody->ToTac(cb, nextIfBody);
    cb->AddInstr(nextIfBody);
    ifBody = ifBody->GetNext();
  }
  cb->AddInstr(new CTacInstr(opGoto, next, NULL, NULL));

  cb->AddInstr(nextFalse);
  while (elseBody) {
    CTacLabel *nextElseBody = cb->CreateLabel();
    elseBody->ToTac(cb, nextElseBody);
    cb->AddInstr(nextElseBody);
    elseBody = elseBody->GetNext();
  }

  cb->AddInstr(new CTacInstr(opGoto, next, NULL, NULL));

  return NULL;
}


//------------------------------------------------------------------------------
// CAstStatWhile
//
CAstStatWhile::CAstStatWhile(CToken t,
                             CAstExpression *cond, CAstStatement *body)
  : CAstStatement(t), _cond(cond), _body(body)
{
  assert(cond != NULL);
}

CAstExpression* CAstStatWhile::GetCondition(void) const
{
  return _cond;
}

CAstStatement* CAstStatWhile::GetBody(void) const
{
  return _body;
}

bool CAstStatWhile::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;
  CAstExpression *cond = GetCondition();
  CAstStatement *body = GetBody();

  // Type check for condition expression
  if (!cond->TypeCheck(t, msg))
    return false;

  // check whether condition's type is boolean type
  if (!cond->GetType() || !cond->GetType()->Match(CTypeManager::Get()->GetBool())) {
    if (t) *t = cond->GetToken();
    if (msg) {
      out << "condition should be bool type, but ";
      if (cond->GetType()) out << cond->GetType();
      else out << "<INVALID>";
      out << " appeared" << endl;
      *msg = out.str();
    }
    return false;
  }

  // Type check for statements in while-body (can be empty)
  while (body) {
    if (!body->TypeCheck(t, msg))
      return false;
    body = body->GetNext();
  }

  return true;
}

ostream& CAstStatWhile::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "while cond" << endl;
  _cond->print(out, indent+2);
  out << ind << "while-body" << endl;
  if (_body != NULL) {
    CAstStatement *s = _body;
    do {
      s->print(out, indent+2);
      s = s->GetNext();
    } while (s != NULL);
  }
  else out << ind << "  empty." << endl;

  return out;
}

string CAstStatWhile::dotAttr(void) const
{
  return " [label=\"while\",shape=box]";
}

void CAstStatWhile::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _cond->toDot(out, indent);
  out << ind << dotID() << "->" << _cond->dotID() << ";" << endl;

  if (_body != NULL) {
    CAstStatement *s = _body;
    if (s != NULL) {
      string prev = dotID();
      do {
        s->toDot(out, indent);
        out << ind << prev << " -> " << s->dotID() << " [style=dotted];"
            << endl;
        prev = s->dotID();
        s = s->GetNext();
      } while (s != NULL);
    }
  }
}

CTacAddr* CAstStatWhile::ToTac(CCodeBlock *cb, CTacLabel *next)
{
  CTacLabel *cond = cb->CreateLabel("while_cond");
  CTacLabel *body = cb->CreateLabel("while_body");
  CAstStatement *bodyStat = GetBody();

  /* The TAC of CAstStatWhile has the form as follwing;
   *
   * while_cond:
   *   if (the condition is true) goto while_body
   *   goto next
   * while_body:
   *   (whileBody statement sequence)
   *   goto while_cond
   * next:
   */

  cb->AddInstr(cond);
  GetCondition()->ToTac(cb, body, next);

  cb->AddInstr(body);
  while (bodyStat) {
    CTacLabel *nextBody = cb->CreateLabel();
    bodyStat->ToTac(cb, nextBody);
    cb->AddInstr(nextBody);
    bodyStat = bodyStat->GetNext();
  }

  cb->AddInstr(new CTacInstr(opGoto, cond, NULL, NULL));
  cb->AddInstr(new CTacInstr(opGoto, next, NULL, NULL));

  return NULL;
}


//------------------------------------------------------------------------------
// CAstExpression
//
CAstExpression::CAstExpression(CToken t)
  : CAstNode(t)
{
}


CTacAddr* CAstExpression::ToTac(CCodeBlock *cb)
{
  return NULL;
}


CTacAddr* CAstExpression::ToTac(CCodeBlock *cb,
                                CTacLabel *ltrue, CTacLabel *lfalse)
{
  // generate jumping code for boolean expression

  return NULL;
}

//------------------------------------------------------------------------------
// CAstOperation
//
CAstOperation::CAstOperation(CToken t, EOperation oper)
  : CAstExpression(t), _oper(oper)
{
}

EOperation CAstOperation::GetOperation(void) const
{
  return _oper;
}


//------------------------------------------------------------------------------
// CAstBinaryOp
//
CAstBinaryOp::CAstBinaryOp(CToken t, EOperation oper,
                           CAstExpression *l,CAstExpression *r)
  : CAstOperation(t, oper), _left(l), _right(r)
{
  // these are the only binary operation we support for now
  assert((oper == opAdd)        || (oper == opSub)         ||
         (oper == opMul)        || (oper == opDiv)         ||
         (oper == opAnd)        || (oper == opOr)          ||
         (oper == opEqual)      || (oper == opNotEqual)    ||
         (oper == opLessThan)   || (oper == opLessEqual)   ||
         (oper == opBiggerThan) || (oper == opBiggerEqual)
        );
  assert(l != NULL);
  assert(r != NULL);
}

CAstExpression* CAstBinaryOp::GetLeft(void) const
{
  return _left;
}

CAstExpression* CAstBinaryOp::GetRight(void) const
{
  return _right;
}

bool CAstBinaryOp::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;
  CAstExpression *lhs = GetLeft(), *rhs = GetRight();
  EOperation oper = GetOperation();
  CTypeManager *tm = CTypeManager::Get();

  if (!lhs->TypeCheck(t,msg) || !rhs->TypeCheck(t,msg))
    return false;

  // binary operation requires that both left type and right type are scalar type
  const CType *lt = lhs->GetType(), *rt = rhs->GetType();
  if (lt == NULL || !lt->IsScalar()) {
    if (t) *t = lhs->GetToken();
    if (msg) {
      out << "the type of left operand is not scalar type." << endl;
      out << "left operand : ";
      if (lt) out << lt << endl;
      else out << "<INVALID>" << endl;
      *msg = out.str();
    }
    return false;
  }
  if (rt == NULL || !rt->IsScalar()) {
    if (t) *t = rhs->GetToken();
    if (msg) {
      out << "the type of right operand is not scalar type." << endl;
      out << "right operand : ";
      if (rt) out << rt << endl;
      else out << "<INVALID>" << endl;
      *msg = out.str();
    }
    return false;
  }

  // check left type and right type is not pointer type
  // remark that pointer type is scalar type
  if (lt->IsPointer()) {
    if (t) *t = lhs->GetToken();
    if (msg) *msg = "the type of left operand cannot be a pointer type";
    return false;
  }
  if (rt->IsPointer()) {
    if (t) *t = rhs->GetToken();
    if (msg) *msg = "the type of right operand cannot be a pointer type";
    return false;
  }

  // binary operation requires that left type equals right type
  if (!lt->Match(rt)) {
    if (t) *t = GetToken();
    if (msg) {
      out << "the type of left operand does not match with the type of right operand." << endl;
      out << "left operand : " << lt << endl;
      out << "right operand : " << rt << endl;
      *msg = out.str();
    }
    return false;
  }

  string soper = "this"; // TODO : match string operation.
  switch (oper) {
    case opAdd:
    case opSub:
    case opMul:
    case opDiv:
      if (!lt->Match(tm->GetInt())) {
        if (t) *t = lhs->GetToken();
        if (msg) {
          out << "the type of operands should be an integer type "
                 "in " << soper << " operation." << endl;
          out << "left operand : " << lt << endl;
          out << "right operand : " << rt << endl;
          *msg = out.str();
        }
        return false;
      }
      break;
    case opAnd:
    case opOr:
      if (!lt->Match(tm->GetBool())) {
        if (t) *t = lhs->GetToken();
        if (msg) {
          out << "the type of operands should be an boolean type "
                 "in " << soper << " operation." << endl;
          out << "left operand : " << lt << endl;
          out << "right operand : " << rt << endl;
          *msg = out.str();
        }
        return false;
      }
      break;
    case opEqual:
    case opNotEqual:
      // when operation is '=' or '#'
      break;
    case opLessThan:
    case opLessEqual:
    case opBiggerThan:
    case opBiggerEqual:
      if (lt->Match(tm->GetBool())) {
        if (t) *t = lhs->GetToken();
        if (msg) {
          out << "the type of operands cannot be boolean type "
                 "in " << soper << " operation." << endl;
          out << "left operand : " << lt << endl;
          out << "right operand : " << rt << endl;
          *msg = out.str();
        }
        return false;
      }
      break;
    default:
      if (t) *t = lhs->GetToken();
      if (msg) {
        out << "the operation is not valid." << endl;
        *msg = out.str();
      }
      return false;
  }

  return true;
}

const CType* CAstBinaryOp::GetType(void) const
{
  const CType *ret;
  EOperation oper = GetOperation();

  switch (oper) {
    case opAdd:
    case opSub:
    case opMul:
    case opDiv:
      ret = CTypeManager::Get()->GetInt();
      break;
    case opAnd:
    case opOr:
    case opEqual:
    case opNotEqual:
    case opLessThan:
    case opLessEqual:
    case opBiggerThan:
    case opBiggerEqual:
      ret = CTypeManager::Get()->GetBool();
      break;
    default:
      ret = NULL;
      break;
  }

  return ret;
}

ostream& CAstBinaryOp::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << GetOperation() << " ";

  const CType *t = GetType();
  if (t != NULL) out << t << endl;
  else out << "<INVALID>" << endl;

  _left->print(out, indent+2);
  _right->print(out, indent+2);

  return out;
}

string CAstBinaryOp::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << GetOperation() << "\",shape=box]";
  return out.str();
}

void CAstBinaryOp::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _left->toDot(out, indent);
  out << ind << dotID() << "->" << _left->dotID() << ";" << endl;
  _right->toDot(out, indent);
  out << ind << dotID() << "->" << _right->dotID() << ";" << endl;
}

CTacAddr* CAstBinaryOp::ToTac(CCodeBlock *cb)
{
  CAstExpression *left = GetLeft(), *right = GetRight();
  EOperation oper = GetOperation();
  CTypeManager *tm = CTypeManager::Get();

  /* when oper == "+", "-", "*", or "/",
   * the expression is an integer type
   */
  if (oper == opAdd || oper == opSub || oper == opMul || oper == opDiv) {
    CTacAddr *leftTac = left->ToTac(cb), *rightTac = right->ToTac(cb);
    CTacTemp *val = cb->CreateTemp(tm->GetInt());
    cb->AddInstr(new CTacInstr(oper, val, leftTac, rightTac));
    return val;
  }

  /* otherwise, the expression is an boolean type */
  CTacLabel *ltrue = cb->CreateLabel(), *lfalse = cb->CreateLabel();
  CTacLabel *lend = cb->CreateLabel();
  ToTac(cb, ltrue, lfalse);

  CTacTemp *val = cb->CreateTemp(tm->GetBool());
  cb->AddInstr(ltrue);
  cb->AddInstr(new CTacInstr(opAssign, val, new CTacConst(1), NULL));
  cb->AddInstr(new CTacInstr(opGoto, lend, NULL, NULL));
  cb->AddInstr(lfalse);
  cb->AddInstr(new CTacInstr(opAssign, val, new CTacConst(0), NULL));
  cb->AddInstr(lend);
  return val;
}

CTacAddr* CAstBinaryOp::ToTac(CCodeBlock *cb,
                              CTacLabel *ltrue, CTacLabel *lfalse)
{
  EOperation oper = GetOperation();
  CAstExpression *left = GetLeft(), *right = GetRight();
  CTacLabel *nextCond = cb->CreateLabel();

  if (IsRelOp(oper)) {
    cb->AddInstr(new CTacInstr(oper, ltrue, left->ToTac(cb), right->ToTac(cb)));
    cb->AddInstr(new CTacInstr(opGoto, lfalse));
  }
  else {
  // short-circuit expression
    if (oper == opAnd) {
      /* If the current condition is true,
       * then look the next condtion.
       *
       * Otherwise, the total condition is false 
       * regardless of the remaining condition.
       */
  
      left->ToTac(cb, nextCond, lfalse);
      cb->AddInstr(nextCond);
      right->ToTac(cb, ltrue, lfalse);
    }
    else {
      /* If the current condition is true,
      * then the total condition is true
      * regardless of the remaining condition.
      *
      * Otherwise, look the next condition.
      */

      left->ToTac(cb, ltrue, nextCond);
      cb->AddInstr(nextCond);
      right->ToTac(cb, ltrue, lfalse);
    }
  }

  return NULL;
}


//------------------------------------------------------------------------------
// CAstUnaryOp
//
CAstUnaryOp::CAstUnaryOp(CToken t, EOperation oper, CAstExpression *e)
  : CAstOperation(t, oper), _operand(e)
{
  assert((oper == opNeg) || (oper == opPos) || (oper == opNot));
  assert(e != NULL);
}

CAstExpression* CAstUnaryOp::GetOperand(void) const
{
  return _operand;
}

bool CAstUnaryOp::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;
  CAstExpression *operand = GetOperand();
  EOperation oper = GetOperation();
  CTypeManager *tm = CTypeManager::Get();

  // check operand's type checking
  if (!operand->TypeCheck(t,msg)) {
    CAstConstant *number = dynamic_cast<CAstConstant*> (operand);
    // ignore type checking failure if its node is -2147483648
    if (number != NULL && oper == opNeg)
      return true;
    return false;
  }

  string soper = "this"; // TODO : match string operation.
  switch (oper) {
    case opNeg:
    case opPos:
      if (!operand->GetType() || !operand->GetType()->Match(tm->GetInt())) {
        if (t) *t = operand->GetToken();
        if (msg) {

          out << "the type of operand should be an integer type "
                 "in " << soper << " operation." << endl;
          out << "operand : ";
          if (operand->GetType()) out << operand->GetType() << endl;
          else out << "<INVALID>" << endl;
          *msg = out.str();
        }
        return false;
      }
      break;
    case opNot:
      if (!operand->GetType() || !operand->GetType()->Match(tm->GetBool())) {
        if (t) *t = operand->GetToken();
        if (msg) {
          out << "the type of operand should be a boolean type "
                 "in " << soper << " operation." << endl;
          out << "operand : ";
          if (operand->GetType()) out << operand->GetType() << endl;
          else out << "<INVALID>" << endl;
          *msg = out.str();
        }
        return false;
      }
      break;
    default:
      if (t) *t = GetToken();
      if (msg) {
        out << "the operation is not valid." << endl;
        *msg = out.str();
      }
      return false;
  }

  return true;
}

const CType* CAstUnaryOp::GetType(void) const
{
  const CType *ret;
  EOperation oper = GetOperation();

  switch (oper) {
    case opNeg:
    case opPos:
      ret = CTypeManager::Get()->GetInt();
      break;
    case opNot:
      ret = CTypeManager::Get()->GetBool();
      break;
    default:
      ret = NULL;
      break;
  }

  return ret;
}

ostream& CAstUnaryOp::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << GetOperation() << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";
  out << endl;

  _operand->print(out, indent+2);

  return out;
}

string CAstUnaryOp::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << GetOperation() << "\",shape=box]";
  return out.str();
}

void CAstUnaryOp::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _operand->toDot(out, indent);
  out << ind << dotID() << "->" << _operand->dotID() << ";" << endl;
}

CTacAddr* CAstUnaryOp::ToTac(CCodeBlock *cb)
{
  EOperation oper = GetOperation();
  CTypeManager *tm = CTypeManager::Get();

  CTacTemp *retval = NULL;

  // If oper == "+' or "-", the expression is an integer type
  if (oper == opPos || oper == opNeg) {
    CAstConstant *number = dynamic_cast<CAstConstant*>(GetOperand());

    if (number == NULL) {
      CTacAddr *operandTac = GetOperand()->ToTac(cb);
      retval = cb->CreateTemp(tm->GetInt());
      cb->AddInstr(new CTacInstr(oper, retval, operandTac));
    }
    else {
      /* For example, if the node is CAstUnaryOp("-", 2147483648),
       * it returns CTacConstant(CAstConstant(-2147483648))
       */

      long long val = number->GetValue();
      if (oper == opNeg)
        val = -val;
      CTacConst *numTac = new CTacConst(val);
      return numTac;
    }
  }
  else {
    CTacLabel *ltrue = cb->CreateLabel(), *lfalse = cb->CreateLabel();
    CTacLabel *lend = cb->CreateLabel();
    ToTac(cb, ltrue, lfalse);

    retval = cb->CreateTemp(tm->GetBool());

    cb->AddInstr(ltrue);
    cb->AddInstr(new CTacInstr(opAssign, retval, new CTacConst(1), NULL));
    cb->AddInstr(new CTacInstr(opGoto, lend));

    cb->AddInstr(lfalse);
    cb->AddInstr(new CTacInstr(opAssign, retval, new CTacConst(0), NULL));
    cb->AddInstr(lend);
  }

  return retval;
}

CTacAddr* CAstUnaryOp::ToTac(CCodeBlock *cb,
                             CTacLabel *ltrue, CTacLabel *lfalse)
{
  EOperation oper = GetOperation();
  CAstExpression *operand = GetOperand();

  assert(oper == opNot);
  operand->ToTac(cb, lfalse, ltrue); // just swaps ltrue and lfalse
  // cb->AddInstr(new CTacInstr(opEqual, lfalse, operand->ToTac(cb), new CTacConst(1)));

  return NULL;
}


//------------------------------------------------------------------------------
// CAstSpecialOp
//
CAstSpecialOp::CAstSpecialOp(CToken t, EOperation oper, CAstExpression *e,
                             const CType *type)
  : CAstOperation(t, oper), _operand(e), _type(type)
{
  assert((oper == opAddress) || (oper == opDeref) || (oper == opCast));
  assert(e != NULL);
  assert(((oper != opCast) && (type == NULL)) ||
         ((oper == opCast) && (type != NULL)));
}

CAstExpression* CAstSpecialOp::GetOperand(void) const
{
  return _operand;
}

bool CAstSpecialOp::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;
  EOperation oper = GetOperation();

  if (!_operand->TypeCheck(t, msg))
    return false;

  // opDeref operation can be valid only when the operand's type is pointer type
  if (oper == opDeref) {
    if (!_operand->GetType() || !_operand->GetType()->IsPointer()) {
      if (t) *t = _operand->GetToken();
      if (msg) {
        out << "the dereference of non-pointer type (";
        if (_operand->GetType()) out << _operand->GetType();
        else out << "<INVALID>";
        out << ") is not allowed." << endl;
        *msg = out.str();
      }
      return false;
    }
  }

  return true;
}

const CType* CAstSpecialOp::GetType(void) const
{
  const CType* ret;
  EOperation oper = GetOperation();

  switch (oper) {
    case opAddress:
      ret = CTypeManager::Get()->GetPointer(GetOperand()->GetType());
      break;
    case opDeref:
      if (GetOperand()->GetType()->IsPointer())
        ret = dynamic_cast<const CPointerType*>(GetOperand()->GetType())->GetBaseType();
      else
        ret = NULL;
      break;
    case opCast:
      ret = _type;
      break;
    default:
      ret = NULL;
      break;
  }

  return ret;
}

ostream& CAstSpecialOp::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << GetOperation() << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";
  out << endl;

  _operand->print(out, indent+2);

  return out;
}

string CAstSpecialOp::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << GetOperation() << "\",shape=box]";
  return out.str();
}

void CAstSpecialOp::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  _operand->toDot(out, indent);
  out << ind << dotID() << "->" << _operand->dotID() << ";" << endl;
}

CTacAddr* CAstSpecialOp::ToTac(CCodeBlock *cb)
{
  CAstExpression *operand = GetOperand();

  CTacAddr *src = operand->ToTac(cb);
  CTacTemp *ret =
    cb->CreateTemp(CTypeManager::Get()->GetPointer(operand->GetType()));

  cb->AddInstr(new CTacInstr(opAddress, ret, src, NULL));

  return ret;
}


//------------------------------------------------------------------------------
// CAstFunctionCall
//
CAstFunctionCall::CAstFunctionCall(CToken t, const CSymProc *symbol)
  : CAstExpression(t), _symbol(symbol)
{
  assert(symbol != NULL);
}

const CSymProc* CAstFunctionCall::GetSymbol(void) const
{
  return _symbol;
}

void CAstFunctionCall::AddArg(CAstExpression *arg)
{
  _arg.push_back(arg);
}

int CAstFunctionCall::GetNArgs(void) const
{
  return (int)_arg.size();
}

CAstExpression* CAstFunctionCall::GetArg(int index) const
{
  assert((index >= 0) && (index < _arg.size()));
  return _arg[index];
}

bool CAstFunctionCall::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;
  CTypeManager *tm = CTypeManager::Get();
  const CSymProc *proc = dynamic_cast<const CSymProc*>(_symbol);

  // check the number of parameters
  if (GetNArgs() != proc->GetNParams()) {
    if (t) *t = GetToken();
    if (msg) {
      out << "the number of parameters mismatched." << endl;
      out << "signature : " << proc->GetNParams() << endl;
      out << "subroutineCall : " << GetNArgs() << endl;
      *msg = out.str();
    }
    return false;
  }

  // type checking for expression and its type is well matched with the signature
  for (int i = 0; i < GetNArgs(); i++) {
    CAstExpression *expr = GetArg(i);
    const CType *paramType = proc->GetParam(i)->GetDataType();

    if (!expr->TypeCheck(t,msg))
      return false;

    if (!expr->GetType() || !paramType ||
        !paramType->Match(expr->GetType())) {
      if (t) *t = expr->GetToken();
      if (msg) {
        out << "the type of parameters does not match "
               "with the function/procedure's signature." << endl;
        out << "signature : ";
        if (paramType) out << paramType << endl;
        else out << "<INVALID>" << endl;
        out << "subroutineCall : ";
        if (expr->GetType()) out << expr->GetType() << endl;
        else out << "<INVALID>" << endl;
        *msg = out.str();
      }
      return false;
    }
  }

  return true;
}

const CType* CAstFunctionCall::GetType(void) const
{
  return GetSymbol()->GetDataType();
}

ostream& CAstFunctionCall::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << "call " << _symbol << " ";
  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";
  out << endl;

  for (size_t i=0; i<_arg.size(); i++) {
    _arg[i]->print(out, indent+2);
  }

  return out;
}

string CAstFunctionCall::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"call " << _symbol->GetName() << "\",shape=box]";
  return out.str();
}

void CAstFunctionCall::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  for (size_t i=0; i<_arg.size(); i++) {
    _arg[i]->toDot(out, indent);
    out << ind << dotID() << "->" << _arg[i]->dotID() << ";" << endl;
  }
}

CTacAddr* CAstFunctionCall::ToTac(CCodeBlock *cb)
{
  // similar to CAstStatCall::ToTac(CCodeBlock *cb)

  int n = GetNArgs();

  for (int i = n - 1; i >= 0; i--) {
      CTacAddr *argTac = GetArg(i)->ToTac(cb);
      CTacInstr *instr = new CTacInstr(opParam, new CTacConst(i), argTac, NULL);
      cb->AddInstr(instr);
  }

  CTacTemp *retval = cb->CreateTemp(GetType());
  cb->AddInstr(new CTacInstr(opCall, retval, new CTacName(GetSymbol()), NULL));

  return retval;
}

CTacAddr* CAstFunctionCall::ToTac(CCodeBlock *cb,
                                  CTacLabel *ltrue, CTacLabel *lfalse)
{
  CTacAddr *retval = ToTac(cb);

  cb->AddInstr(new CTacInstr(opEqual, ltrue, retval, new CTacConst(1)));
  cb->AddInstr(new CTacInstr(opGoto, lfalse, NULL, NULL));

  return NULL;
}



//------------------------------------------------------------------------------
// CAstOperand
//
CAstOperand::CAstOperand(CToken t)
  : CAstExpression(t)
{
}


//------------------------------------------------------------------------------
// CAstDesignator
//
CAstDesignator::CAstDesignator(CToken t, const CSymbol *symbol)
  : CAstOperand(t), _symbol(symbol)
{
  assert(symbol != NULL);
}

const CSymbol* CAstDesignator::GetSymbol(void) const
{
  return _symbol;
}

bool CAstDesignator::TypeCheck(CToken *t, string *msg) const
{
  if (GetType() == NULL || GetType()->IsNull()) {
    if (t) *t = GetToken();
    if (msg) *msg = "invalid designator type.";
    return false;
  }

  return true;
}

const CType* CAstDesignator::GetType(void) const
{
  // just returns the symbol's datatype
  return GetSymbol()->GetDataType();
}

ostream& CAstDesignator::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << _symbol << " ";

  const CType *t = GetType();
  if (t) out << t << endl;
  else out << "<INVALID>" << endl;

  return out;
}

string CAstDesignator::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << _symbol->GetName();
  out << "\",shape=ellipse]";
  return out.str();
}

void CAstDesignator::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);
}

CTacAddr* CAstDesignator::ToTac(CCodeBlock *cb)
{
  return new CTacName(GetSymbol());
}

CTacAddr* CAstDesignator::ToTac(CCodeBlock *cb,
                                CTacLabel *ltrue, CTacLabel *lfalse)
{
  cb->AddInstr(new CTacInstr(opEqual, ltrue, ToTac(cb), new CTacConst(1)));
  cb->AddInstr(new CTacInstr(opGoto, lfalse, NULL, NULL));

  return NULL;
}


//------------------------------------------------------------------------------
// CAstArrayDesignator
//
CAstArrayDesignator::CAstArrayDesignator(CToken t, const CSymbol *symbol)
  : CAstDesignator(t, symbol), _done(false), _offset(NULL)
{
}

void CAstArrayDesignator::AddIndex(CAstExpression *idx)
{
  assert(!_done);
  _idx.push_back(idx);
}

void CAstArrayDesignator::IndicesComplete(void)
{
  assert(!_done);
  _done = true;
}

int CAstArrayDesignator::GetNIndices(void) const
{
  return (int)_idx.size();
}

CAstExpression* CAstArrayDesignator::GetIndex(int index) const
{
  assert((index >= 0) && (index < _idx.size()));
  return _idx[index];
}

bool CAstArrayDesignator::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;
  CTypeManager *tm = CTypeManager::Get();
  bool result = true;
  assert(_done);

  for (int i = 0; i < GetNIndices(); i++) {
    CAstExpression *expr = GetIndex(i);

    // type checking for expression
    if (!expr->TypeCheck(t,msg))
      return false;

    // check whether its type is integer type
    if (!expr->GetType() || !expr->GetType()->Match(tm->GetInt())) {
      if (t) *t = expr->GetToken();
      if (msg) {
        out << "the element in array should be accessed by integer index." << endl;
        out << "but the expression's type is ";
        if (expr->GetType()) out << expr->GetType() << endl;
        else out << "<INVALID>" << endl;
        *msg = out.str();
      }
      return false;
    }
  }

  return result;
}

const CType* CAstArrayDesignator::GetType(void) const
{
  const CType *symbolType = GetSymbol()->GetDataType();

  // implicit dereferncing pointer to array
  if (symbolType->IsPointer())
    symbolType = dynamic_cast<const CPointerType*>(symbolType)->GetBaseType();

  if (!symbolType->IsArray())
    return NULL;

  const CType *ret = symbolType;
  if (GetNIndices() > dynamic_cast<const CArrayType*>(symbolType)->GetNDim())
    return NULL;

  for (int i = 0; i < GetNIndices(); i++) {
    if (!ret->IsArray()) {
      ret = NULL;
      break;
    }

    ret = dynamic_cast<const CArrayType*>(ret)->GetInnerType();
  }

  return ret;
}

ostream& CAstArrayDesignator::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << _symbol << " ";

  const CType *t = GetType();
  if (t != NULL) out << t; else out << "<INVALID>";

  out << endl;

  for (size_t i=0; i<_idx.size(); i++) {
    _idx[i]->print(out, indent+2);
  }

  return out;
}

string CAstArrayDesignator::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << _symbol->GetName() << "[]\",shape=ellipse]";
  return out.str();
}

void CAstArrayDesignator::toDot(ostream &out, int indent) const
{
  string ind(indent, ' ');

  CAstNode::toDot(out, indent);

  for (size_t i=0; i<_idx.size(); i++) {
    _idx[i]->toDot(out, indent);
    out << ind << dotID() << "-> " << _idx[i]->dotID() << ";" << endl;
  }
}

CTacAddr* CAstArrayDesignator::ToTac(CCodeBlock *cb)
{
  CToken t;
  CTypeManager *tm = CTypeManager::Get();
  CSymtab *st = cb->GetOwner()->GetSymbolTable();

  // prepare DIM
  const CSymbol *DIM_SYM = st->FindSymbol("DIM");
  CAstConstant *DIM_VAL = new CAstConstant(t, tm->GetInt(), 0);

  // prepare DOFS
  const CSymbol *DOFS_SYM = st->FindSymbol("DOFS");

  // get array identifier properties
  CTacAddr *id = new CTacName(GetSymbol());
  CAstExpression *idExpr = new CAstDesignator(GetToken(), GetSymbol());
  const CArrayType *dataType;

  // referencing pointer
  if (GetSymbol()->GetDataType()->IsPointer()) {
    dataType =
      dynamic_cast<const CArrayType*>
      (dynamic_cast<const CPointerType*>
       (GetSymbol()->GetDataType())->GetBaseType());
  }
  else {
    CTacTemp *ptr = cb->CreateTemp(tm->GetPointer(GetSymbol()->GetDataType()));
    cb->AddInstr(new CTacInstr(opAddress, ptr, id, NULL));
    id = ptr;

    idExpr = new CAstSpecialOp(GetToken(), opAddress, idExpr, NULL);

    dataType =
      dynamic_cast<const CArrayType*>
      (GetSymbol()->GetDataType());
  }
  int dataSize = dataType->GetBaseType()->GetSize();

  CTacAddr *idx = NULL;
  int iterateCount = dataType->GetNDim();
  for (int i = 0; i < iterateCount; i++) {
    // evaluate index
    if (!idx)
      idx = GetIndex(i)->ToTac(cb);
    else {
      CTacAddr *prev = new CTacConst(0);

      if (i < GetNIndices())
        prev = GetIndex(i)->ToTac(cb);

      CTacAddr *next = cb->CreateTemp(tm->GetInt());
      cb->AddInstr(new CTacInstr(opAdd, next, idx, prev));
      idx = next;
    }

    if (i == iterateCount - 1)
      break;

    // call dimention size
    CAstFunctionCall *DIM_FUN =
      new CAstFunctionCall(t, dynamic_cast<const CSymProc*>(DIM_SYM));

    DIM_FUN->AddArg(idExpr);
    DIM_VAL->SetValue(i + 2);
    DIM_FUN->AddArg(DIM_VAL);
    CTacAddr *entrySize = DIM_FUN->ToTac(cb);

    // multiply dimention size
    CTacAddr *next = cb->CreateTemp(tm->GetInt());
    cb->AddInstr(new CTacInstr(opMul, next, idx, entrySize));
    idx = next;
  }

  // multiply data size
  CTacTemp *tmp = cb->CreateTemp(tm->GetInt());
  cb->AddInstr(new CTacInstr(opMul, tmp, idx, new CTacConst(dataSize)));
  idx = tmp;

  // calculate array offset
  CAstFunctionCall *DOFS_FUN =
    new CAstFunctionCall(t, dynamic_cast<const CSymProc*>(DOFS_SYM));

  DOFS_FUN->AddArg(idExpr);
  CTacAddr *ofs = DOFS_FUN->ToTac(cb);

  tmp = cb->CreateTemp(tm->GetInt());
  cb->AddInstr(new CTacInstr(opAdd, tmp, idx, ofs));
  idx = tmp;

  tmp = cb->CreateTemp(tm->GetInt());
  cb->AddInstr(new CTacInstr(opAdd, tmp, id, idx));

  return new CTacReference(tmp->GetSymbol(), GetSymbol());
}

CTacAddr* CAstArrayDesignator::ToTac(CCodeBlock *cb,
                                     CTacLabel *ltrue, CTacLabel *lfalse)
{
  CTacAddr *ret = ToTac(cb);

  cb->AddInstr(new CTacInstr(opEqual, ltrue, ret, new CTacConst(1)));
  cb->AddInstr(new CTacInstr(opGoto, lfalse, NULL, NULL));

  return ret;
}


//------------------------------------------------------------------------------
// CAstConstant
//
CAstConstant::CAstConstant(CToken t, const CType *type, long long value)
  : CAstOperand(t), _type(type), _value(value)
{
}

void CAstConstant::SetValue(long long value)
{
  _value = value;
}

long long CAstConstant::GetValue(void) const
{
  return _value;
}

string CAstConstant::GetValueStr(void) const
{
  ostringstream out;

  if (GetType() == CTypeManager::Get()->GetBool())
    out << (_value == 0 ? "false" : "true");
  else
    out << dec << _value;

  return out.str();
}

bool CAstConstant::TypeCheck(CToken *t, string *msg) const
{
  ostringstream out;

  if (_type == NULL || _type->IsNull()) {
    if (t) *t = GetToken();
    if (msg) *msg = "invalid constant type.";
    return false;
  }

  // type check fails if the value is 2147483648
  // note that this failure can be ignored by CAstUnaryOp,
  // especially the node is unary("-", constant(2147483648))
  if (GetValue() == (1LL << 31)) {
    if (t) *t = GetToken();
    if (msg) {
      out << "invalid number. (" << GetValue() << ")" << endl;
      *msg = out.str();
    }
    return false;
  }

  return true;
}

const CType* CAstConstant::GetType(void) const
{
  return _type;
}

ostream& CAstConstant::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << GetValueStr() << " ";

  const CType *t = GetType();
  if (t) out << t << endl;
  else out << "<INVALID>" << endl;

  return out;
}

string CAstConstant::dotAttr(void) const
{
  ostringstream out;
  out << " [label=\"" << GetValueStr() << "\",shape=ellipse]";
  return out.str();
}

CTacAddr* CAstConstant::ToTac(CCodeBlock *cb)
{
  /* 1<<31 What should we do oh my god?
   *
   * calm down. this is egger's trap card.
   * when 1<<31, it will be specially treated 
   * in CAstUnaryOp::ToTac(CCodeBlock *cb).
   */
  return new CTacConst(GetValue());
}

CTacAddr* CAstConstant::ToTac(CCodeBlock *cb,
                                CTacLabel *ltrue, CTacLabel *lfalse)
{
  long long cond = GetValue();

  if (cond)
    cb->AddInstr(new CTacInstr(opGoto, ltrue, NULL, NULL));
  else
    cb->AddInstr(new CTacInstr(opGoto, lfalse, NULL, NULL));

  return new CTacConst(GetValue());
}


//------------------------------------------------------------------------------
// CAstStringConstant
//
int CAstStringConstant::_idx = 0;

CAstStringConstant::CAstStringConstant(CToken t, const string value,
                                       CAstScope *s)
  : CAstOperand(t)
{
  CTypeManager *tm = CTypeManager::Get();

  _type = tm->GetArray(strlen(CToken::unescape(value).c_str())+1,
                       tm->GetChar());
  _value = new CDataInitString(value);

  ostringstream o;
  o << "_str_" << ++_idx;

  _sym = new CSymGlobal(o.str(), _type);
  _sym->SetData(_value);
  s->GetSymbolTable()->AddSymbol(_sym);
}

const string CAstStringConstant::GetValue(void) const
{
  return _value->GetData();
}

const string CAstStringConstant::GetValueStr(void) const
{
  return GetValue();
}

bool CAstStringConstant::TypeCheck(CToken *t, string *msg) const
{
  if (_type == NULL || _type->IsNull()) {
    if (t) *t = GetToken();
    if (msg) *msg = "invalid string constant type.";
    return false;
  }

  return true;
}

const CType* CAstStringConstant::GetType(void) const
{
  return _type;
}

ostream& CAstStringConstant::print(ostream &out, int indent) const
{
  string ind(indent, ' ');

  out << ind << '"' << GetValueStr() << '"' << " ";

  const CType *t = GetType();
  if (t) out << t << endl;
  else out << "<INVALID>" << endl;

  return out;
}

string CAstStringConstant::dotAttr(void) const
{
  ostringstream out;
  // the string is already escaped, but dot requires double escaping
  out << " [label=\"\\\"" << CToken::escape(GetValueStr())
      << "\\\"\",shape=ellipse]";
  return out.str();
}

CTacAddr* CAstStringConstant::ToTac(CCodeBlock *cb)
{
  return new CTacName(_sym);
}

CTacAddr* CAstStringConstant::ToTac(CCodeBlock *cb,
                                CTacLabel *ltrue, CTacLabel *lfalse)
{
  return new CTacName(_sym);
}

