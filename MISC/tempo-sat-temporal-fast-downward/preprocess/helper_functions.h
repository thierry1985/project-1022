#ifndef HELPERS_H
#define HELPERS_H

#include "state.h"
#include "variable.h"
#include "successor_generator.h"
#include "causal_graph.h"

#include <string>
#include <vector>
#include <iostream>

using namespace std;

class State;
class Operator;
class Axiom_relational;
class Axiom_functional;
class DomainTransitionGraph;

//void read_everything
void read_preprocessed_problem_description(istream &in,
    vector<Variable> &internal_variables, vector<Variable *> &variables,
    State &initial_state, vector<pair<Variable*, int> > &goals,
    vector<Operator> &operators, vector<Axiom_relational> &axioms_rel,
    vector<Axiom_functional> &axioms_func);

//void dump_everything
void dump_preprocessed_problem_description(const vector<Variable *> &variables,
    const State &initial_state, const vector<pair<Variable*, int> > &goals,
    const vector<Operator> &operators,
    const vector<Axiom_relational> &axioms_rel,
    const vector<Axiom_functional> &axioms_func);

void dump_DTGs(const vector<Variable *> &ordering,
    vector<DomainTransitionGraph*> &transition_graphs);

void generate_cpp_input(bool causal_graph_acyclic,
    const vector<Variable *> & ordered_var, const State &initial_state,
    const vector<pair<Variable*, int> > &goals,
    const vector<Operator> & operators,
    const vector<Axiom_relational> &axioms_rel,
    const vector<Axiom_functional> &axioms_func, const SuccessorGenerator &sg,
    const vector<DomainTransitionGraph*> transition_graphs,
    const CausalGraph &cg);

void check_magic(istream &in, string magic);

enum foperator {assign=0, scale_up=1, scale_down=2, increase=3, decrease=4, lt=5, le=6, eq=7, ge=8, gt=9, ue=10};

istream& operator>>(istream &is, foperator &fop);

ostream& operator<<(ostream &os, const foperator &fop);

foperator get_inverse_op(foperator op);

struct DurationCond {
  foperator op;
  Variable *var;
  DurationCond(foperator o, Variable *v) :
    op(o), var(v) {
  }
  DurationCond() {}
};

template<typename T> vector<T> append(vector<T> &first, vector<T> &sec) {
  for(int i=0; i< sec.size(); i++) {
    first.push_back(sec[i]);
  }
  return first;
}

#endif
