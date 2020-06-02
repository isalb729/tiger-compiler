#include "straightline/slp.h"

namespace A {
int A::CompoundStm::MaxArgs() const {
  return MAX(stm1->MaxArgs(), stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  return stm2->Interp(stm1->Interp(t));
}

int A::AssignStm::MaxArgs() const {
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  IntAndTable *r = exp->Interp(t);
  if (r->t == nullptr) {
    return new Table(id, r->i, nullptr);
  } else
    return r->t->Update(id, r->i);
}

int A::PrintStm::MaxArgs() const {
  return MAX(exps->MaxArgs(), exps->NumExps());
}

Table *A::PrintStm::Interp(Table *t) const {
  ExpList *fexps = exps;
  IntAndTable *r = fexps->Interp(t);
  std::cout<<r->i;
  t = r->t;
  while (fexps = fexps->getTail()) {
    r = fexps->Interp(t);
    t = r->t;
    std::cout<<" "<<r->i;
  }
  std::cout<<std::endl;
  return r->t;
}

IntAndTable *A::IdExp::Interp(Table *t) const {
  return new IntAndTable(t->Lookup(id), t);  
}

int A::IdExp::MaxArgs() const {
  return 0;
}


IntAndTable *A::NumExp::Interp(Table *t) const {
  return new IntAndTable(num, t);  
}

int A::NumExp::MaxArgs() const {
  return 0;
}

IntAndTable *A::OpExp::Interp(Table *t) const {
  IntAndTable *lt = left->Interp(t);
  IntAndTable *rt = right->Interp(lt->t);
  int value = 0;
  switch (oper)
  {
  case PLUS:
    value = lt->i + rt->i;
    break;
  case MINUS:
    value = lt->i - rt->i;
    break;
  case TIMES:
    value = lt->i * rt->i;
    break;
  case DIV:
    if (rt->i == 0) assert(false);
    value = lt->i / rt->i;
    break;
  default:
    assert(false);
  }
  return new IntAndTable(value , rt->t);  
}

int A::OpExp::MaxArgs() const {
  return MAX(left->MaxArgs(), right->MaxArgs());
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
  Table *rs = stm->Interp(t);
  return exp->Interp(rs);
}

int A::EseqExp::MaxArgs() const {
  return stm->MaxArgs();
}

int A::PairExpList::NumExps() const {
  return tail->NumExps() + 1;
}

int A::PairExpList::MaxArgs() const {
  return MAX(head->MaxArgs(), tail->MaxArgs());
}

IntAndTable *A::PairExpList::Interp(Table *t) const {
  // only interpret the head
  return head->Interp(t);
}

ExpList *A::PairExpList::getTail() const {
  return tail;
}

int A::LastExpList::NumExps() const {
  return 1;
}

int A::LastExpList::MaxArgs() const {
  return last->MaxArgs();
}

IntAndTable *A::LastExpList::Interp(Table *t) const {
  return last->Interp(t);
}

ExpList *A::LastExpList::getTail() const {
  return nullptr;
}
int Table::Lookup(std::string key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(std::string key, int value) const {

  return new Table(key, value, this);
}
}  // namespace A