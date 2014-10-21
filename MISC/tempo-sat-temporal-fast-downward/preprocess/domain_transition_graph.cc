#include "domain_transition_graph.h"
#include "domain_transition_graph_symb.h"
#include "domain_transition_graph_func.h"
#include "domain_transition_graph_subterm.h"
#include "operator.h"
#include "axiom.h"
#include "variable.h"

#include <algorithm>
#include <cassert>
#include <iostream>
using namespace std;

void DomainTransitionGraph::addTransition(int from, int to, const Operator &op,
    int op_index, trans_type type, vector<Variable *> variables) {
  cout << "virtual method DomainTransitionGraph::addTransition called - error!"
      << endl;
  exit(1);
}

void DomainTransitionGraph::addAxRelTransition(int from, int to,
    const Axiom_relational &ax, int ax_index) {
  cout
      << "virtual method DomainTransitionGraph::addAxRelTransition called - error!"
      << endl;
  exit(1);
}

void DomainTransitionGraph::setRelation(Variable* left_var, foperator op,
    Variable* right_var) {
  cout << "virtual method DomainTransitionGraph::setRelation called - error!"
      << endl;
  exit(1);
}

void DomainTransitionGraph::finalize() {
  cout << "virtual method DomainTransitionGraph::finalize called - error!"
      << endl;
  exit(1);
}

bool DomainTransitionGraph::is_strongly_connected() const {
  cout
      << "virtual method DomainTransitionGraph::is_strongly_connected called - error!"
      << endl;
  exit(1);
}

void DomainTransitionGraph::dump() const {
  cout << "virtual method DomainTransitionGraph::dump called - error!" << endl;
  exit(1);
}

void DomainTransitionGraph::generate_cpp_input(ofstream &outfile) const {
  cout
      << "virtual method DomainTransitionGraph::generate_cpp_input called - error!"
      << endl;
  exit(1);
}

void build_DTGs(const vector<Variable *> &var_order,
    const vector<Operator> &operators,
    const vector<Axiom_relational> &axioms_rel,
    const vector<Axiom_functional> &axioms_func,
    vector<DomainTransitionGraph *> &transition_graphs) {

  for(int i = 0; i < var_order.size(); i++) {
    Variable v = *var_order[i];
    if(v.is_subterm()||v.is_comparison()) {
      DomainTransitionGraphSubterm *dtg = new DomainTransitionGraphSubterm(v);
      transition_graphs.push_back(dtg);
    } else if(v.is_functional()) {
      DomainTransitionGraphFunc *dtg = new DomainTransitionGraphFunc(v);
      transition_graphs.push_back(dtg);
    } else {
      DomainTransitionGraphSymb *dtg = new DomainTransitionGraphSymb(v);
      transition_graphs.push_back(dtg);
    }
  }

  for(int i = 0; i < operators.size(); i++) {
    const Operator &op = operators[i];
    const vector<Operator::PrePost> &pre_post_start = op.get_pre_post_start();
    const vector<Operator::PrePost> &pre_post_end = op.get_pre_post_end();
    vector<Operator::PrePost> pre_posts_to_add;
    for(int j = 0; j < pre_post_start.size(); ++j) {
      pre_posts_to_add.push_back(pre_post_start[j]);
    }
    bool pre_post_overwritten = false;
    for(int j = 0; j < pre_post_end.size(); ++j) {
      for(int k = 0; k < pre_posts_to_add.size(); ++k) {
	if(pre_post_end[j].var == pre_posts_to_add[k].var) {
	  pre_post_overwritten = true;
	  pre_posts_to_add[k].post = pre_post_end[j].post;
	  break;
	}
    }
      if(!pre_post_overwritten) {
	pre_posts_to_add.push_back(pre_post_end[j]);
      }
      pre_post_overwritten = false;
    }
    for(int j = 0; j < pre_posts_to_add.size(); j++) {
      if(pre_posts_to_add[j].pre == pre_posts_to_add[j].post)
	continue;
      const Variable *var = pre_posts_to_add[j].var;
      assert(var->get_layer() == -1);
      int var_level = var->get_level();
      if(var_level != -1) {
	int pre = pre_posts_to_add[j].pre;
	int post = pre_posts_to_add[j].post;
	if(pre != -1) {
	  transition_graphs[var_level]->addTransition(pre, post, op, i,
	      DomainTransitionGraph::start, var_order);
	} else {
	  for(int pre = 0; pre < var->get_range(); pre++)
	    if(pre != post) {
	      transition_graphs[var_level]->addTransition(pre, post, op, i,
		  DomainTransitionGraph::start, var_order);
	    }
	}
      }
      //else
      //cout <<"leave out var "<< var->get_name()<<" (unimportant) " << endl;
    }
    const vector<Operator::NumericalEffect> &numerical_effs_start =
	op.get_numerical_effs_start();
    for(int j = 0; j < numerical_effs_start.size(); j++) {
      const Variable *var = numerical_effs_start[j].var;
      int var_level = var->get_level();
      if(var_level != -1) {
	int foperator = static_cast<int>(numerical_effs_start[j].fop);
	int right_var = numerical_effs_start[j].foperand->get_level();
	transition_graphs[var_level]->addTransition(foperator, right_var, op,
	    i, DomainTransitionGraphSymb::start, var_order);
      }
    }
    const vector<Operator::NumericalEffect> &numerical_effs_end =
	op.get_numerical_effs_end();
    for(int j = 0; j < numerical_effs_end.size(); j++) {
      const Variable *var = numerical_effs_end[j].var;
      int var_level = var->get_level();
      if(var_level != -1) {
	int foperator = static_cast<int>(numerical_effs_end[j].fop);
	int right_var = numerical_effs_end[j].foperand->get_level();
	transition_graphs[var_level]->addTransition(foperator, right_var, op,
	    i, DomainTransitionGraphSymb::end, var_order);
      }
    }
  }

  for(int i = 0; i < axioms_rel.size(); i++) {
    const Axiom_relational &ax = axioms_rel[i];
    Variable *var = ax.get_effect_var();
    int var_level = var->get_level();
    assert(var->get_layer()> -1);
    assert(var_level != -1);
    int old_val = ax.get_old_val();
    int new_val = ax.get_effect_val();
    transition_graphs[var_level]->addAxRelTransition(old_val, new_val, ax, i);
  }

  for(int i = 0; i < axioms_func.size(); i++) {
    const Axiom_functional &ax = axioms_func[i];
    Variable *var = ax.get_effect_var();
    if(!var->is_necessary()&&!var->is_used_in_duration_condition())
      continue;
    int var_level = var->get_level();
    assert(var->get_layer()> -1);
    Variable* left_var = ax.get_left_var();
    Variable* right_var = ax.get_right_var();
    foperator op = ax.get_operator();
    transition_graphs[var_level]->setRelation(left_var, op, right_var);
  }

  for(int i = 0; i < transition_graphs.size(); i++)
    transition_graphs[i]->finalize();
}

bool are_DTGs_strongly_connected(
    const vector<DomainTransitionGraph*> &transition_graphs) {
  bool connected = true;
  // no need to test last variable's dtg (highest level variable)
  for(int i = 0; i < transition_graphs.size() - 1; i++)
    if(!transition_graphs[i]->is_strongly_connected())
      connected = false;
  return connected;
}
