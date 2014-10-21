#ifndef DOMAIN_TRANSITION_GRAPH_H
#define DOMAIN_TRANSITION_GRAPH_H

#include <set>
#include "operator.h"
using namespace std;

class Axiom_relational;
class Variable;

class DomainTransitionGraph {
public:
  typedef set<pair<const Variable *, double> > SetEdgeCondition;
  typedef vector<pair<const Variable *, double> > EdgeCondition;
  enum trans_type {start=0, end=1, ax_rel=2, ax_comp=3, ax_func=4};

  virtual ~DomainTransitionGraph() {};
  
  virtual void addTransition(int from, int to, const Operator &op,
      int op_index, trans_type type, vector<Variable *> variables);
  virtual void addAxRelTransition(int from, int to, const Axiom_relational &ax,
      int ax_index);
  virtual void
      setRelation(Variable* left_var, foperator op, Variable* right_var);
  virtual void finalize();
  virtual void dump() const;
  virtual void generate_cpp_input(ofstream &outfile) const;
  virtual bool is_strongly_connected() const;

};

extern void build_DTGs(const vector<Variable *> &varOrder,
    const vector<Operator> &operators, const vector<Axiom_relational> &axioms,
    const vector<Axiom_functional> &axioms_func,
    vector<DomainTransitionGraph*> &transition_graphs);
extern bool are_DTGs_strongly_connected(
    const vector<DomainTransitionGraph*> &transition_graphs);

#endif
