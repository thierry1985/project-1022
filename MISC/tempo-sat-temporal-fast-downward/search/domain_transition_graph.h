#ifndef DOMAIN_TRANSITION_GRAPH_H
#define DOMAIN_TRANSITION_GRAPH_H

#include <iostream>
#include <map>
#include <vector>
#include <tr1/unordered_map>
//#include "globals.h"
#include "operator.h"
using namespace std;

class CGHeuristic;
class TimeStampedState;
class Operator;

class ValueNode;
class ValueTransition;
class ValueTransitionLabel;
class DomainTransitionGraph;

// Note: We do not use references but pointers to refer to the "parents" of
// transitions and nodes. This is because these structures could not be
// put into vectors otherwise.

typedef multimap<int, ValueNode *> Heap;
typedef std::tr1::unordered_map<int, int> hashmap; 
typedef hashmap::value_type ValuePair;

struct PrevailCondition {
    DomainTransitionGraph *prev_dtg;
    int local_var;
    double value;
    enum var_type {discrete_var = 0, numerical_var = 1} type;
    PrevailCondition(DomainTransitionGraph *prev, int var, double val,
	    var_type the_type) :
	prev_dtg(prev), local_var(var), value(val), type(the_type) {
    }
    PrevailCondition(istream &in);
    void dump() const;
};

struct ValueTransitionLabel {
    int duration_variable;
    Operator *op;
    vector<PrevailCondition> all_prevails;
    ValueTransitionLabel(int the_duration_variable, Operator *theOp,
	    const vector<PrevailCondition> &the_prevails) :
	duration_variable(the_duration_variable), op(theOp),
		all_prevails(the_prevails) {
    }

    //for goals
    ValueTransitionLabel(int the_duration_variable,
	    const vector<PrevailCondition> &the_prevails) :
	duration_variable(the_duration_variable), all_prevails(the_prevails) {
	op = NULL;
    }
    void dump() const;
};

struct ValueTransition {
    ValueNode *target;
    vector<ValueTransitionLabel> labels;
    vector<ValueTransitionLabel> ccg_labels; // labels for cyclic CG heuristic

    ValueTransition(ValueNode *targ) :
	target(targ) {
    }
    void dump() const;
    void simplify();
};

struct ValueNode {
    DomainTransitionGraph *parent_graph;
    int value;
    vector<ValueTransition> transitions;

    vector<int> distances; // cg; empty vector if not yet requested
    vector<ValueTransitionLabel *> helpful_transitions;
    // cg; empty vector if not requested
    Heap::iterator pos_in_heap; // cg
    vector<double> children_state; // cg
    ValueNode *reached_from; // cg
    //ValueNode *helpful_reached_from;     // cg
    ValueTransitionLabel *reached_by; // cg
    //ValueTransitionLabel *helpful_reached_by; // cg

    ValueNode(DomainTransitionGraph *parent, int val) :
	parent_graph(parent), value(val), reached_from(0), reached_by(0) {
    }
    void dump() const;
};

struct CompTransition {
    int left_var;
    int right_var;
    binary_op op;

    void dump() const;
};

struct FuncTransition {
    int starting_variable;
    vector<PrevailCondition> all_prevails;
    assignment_op a_op;
    int influencing_variable;
    int duration_variable;
    Operator *op;
    FuncTransition(int the_starting_variable,
	    vector<PrevailCondition> the_prevails, assignment_op the_a_op,
	    int the_influencing_variable, int the_duration_variable,
	    Operator *the_op) :
	starting_variable(the_starting_variable), all_prevails(the_prevails),
		a_op(the_a_op), influencing_variable(the_influencing_variable),
		duration_variable(the_duration_variable), op(the_op) {
    }
};

class DomainTransitionGraph {
    friend class CGHeuristic;
    friend class ValueNode;
    friend class ValueTransition;
    friend class Transition;

public:
    int var;
    bool is_axiom;

    int last_helpful_transition_extraction_time; // cg heuristic; "dirty bit"

    vector<int> local_to_global_child;
    // used for mapping variables in conditions to their global index 
    // (only needed for initializing child_state for the start node?)
    vector<int> ccg_parents;
    // Same as local_to_global_child, but for cyclic CG heuristic.
    hashmap global_to_local_ccg_parents;

public:

    virtual ~DomainTransitionGraph() {
    }
    static void read_all(istream &in);
    virtual void read_data(istream &in) = 0;
    static void compute_causal_graph_parents_comp(int var, map<int, int> &global_to_ccg_parent);
    static void collect_func_transitions(int var, map<int, int> &global_to_ccg_parent);
    virtual void dump() const = 0;
};

class DomainTransitionGraphSymb : public DomainTransitionGraph {
    friend class CGHeuristic;
    friend class ValueNode;
    friend class ValueTransition;
    friend class Transition;
public:
    vector<ValueNode> nodes;

    DomainTransitionGraphSymb(const DomainTransitionGraphSymb &other); // copying forbidden

public:
    DomainTransitionGraphSymb(int var_index, int node_count);
    virtual void read_data(istream &in);
    virtual void dump() const;
    virtual void get_successors(int value, vector<int> &result) const;
    // Build vector of values v' such that there is a transition from value to v'.
};

class DomainTransitionGraphSubterm : public DomainTransitionGraph {
    friend class CGHeuristic;
    friend class ValueNode;
    friend class ValueTransition;
    friend class Transition;

    int left_var;
    int right_var;
    binary_op op;

    DomainTransitionGraphSubterm(const DomainTransitionGraphSubterm &other); // copying forbidden

public:
    DomainTransitionGraphSubterm(int var_index);
    virtual void read_data(istream &in);
    virtual void dump() const;
    binary_op get_op() const {
	return op;
    }
    int get_left_var() const {
	return left_var;
    }
    int get_right_var() const {
	return right_var;
    }

};

class DomainTransitionGraphFunc : public DomainTransitionGraph {
    friend class CGHeuristic;
    friend class ValueNode;
    friend class ValueTransition;
    friend class Transition;
    friend class LocalProblemNumerical;
    friend class DomainTransitionGraphComp;

    vector<FuncTransition> transitions;

    DomainTransitionGraphFunc(const DomainTransitionGraphFunc &other); // copying forbidden

public:
    DomainTransitionGraphFunc(int var_index);
    virtual void read_data(istream &in);
    virtual void dump() const;
};

class DomainTransitionGraphComp : public DomainTransitionGraph {
    friend class CGHeuristic;
    friend class ValueNode;
    friend class ValueTransition;
    friend class Transition;
    friend class LocalProblemComp;

    //all transitions which modify a primitve functional variable which is a child
    //in the causal graph of this variable
    vector<FuncTransition> transitions;

    DomainTransitionGraphComp(const DomainTransitionGraphComp &other); // copying forbidden
public:
    std::pair<CompTransition, CompTransition> nodes; //first has start value false; second has start value true
    DomainTransitionGraphComp(int var_index);
    virtual void read_data(istream &in);
    void compute_recursively_parents(int var, map<int, int> &global_to_ccg_parent);
    void collect_recursively_func_transitions(int var, map<int, int> &global_to_ccg_parent);
    int translate_global_to_local(map<int, int> &global_to_ccg_parent, int global_var);
    virtual void dump() const;
};

#endif
