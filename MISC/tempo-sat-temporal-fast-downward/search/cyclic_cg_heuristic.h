#ifndef CYCLIC_CG_HEURISTIC_H
#define CYCLIC_CG_HEURISTIC_H

#include <vector>
#include <tr1/unordered_map>
#include "heuristic.h"
#include "relaxed_state.h"
#include "domain_transition_graph.h"
#include <ext/slist>
#include <cmath>

class FuncTransition;
class TimeStampedState;

class LocalProblemDiscrete;
class LocalProblemComp;
class LocalProblemNodeDiscrete;
class LocalProblemNodeComp;
class CyclicCGHeuristic;
class LocalProblem;
class LocalTransitionComp;
class LocalTransitionDiscrete;

typedef __gnu_cxx::slist<std::pair<LocalTransitionDiscrete*,int> >
	waiting_discrete_t;
typedef waiting_discrete_t::const_iterator const_iterator_discrete;
typedef waiting_discrete_t::iterator iterator_discrete;

typedef __gnu_cxx::slist<std::pair<LocalTransitionComp*,int> > waiting_comp_t;
typedef waiting_comp_t::const_iterator const_iterator_comp;
typedef waiting_comp_t::iterator iterator_comp;

typedef std::tr1::unordered_map<int, int> hashmap; 
typedef hashmap::value_type ValuePair;
//*****************************************************
//general classes
//*****************************************************
typedef std::vector<std::pair<__gnu_cxx::slist<int>,Interval> > ChildrenState;

class LocalProblemNode {
    friend class CyclicCGHeuristic;
    friend class LocalProblem;
    friend class LocalTransitionDiscrete;
    friend class LocalTransitionComp;
    friend class LocalProblemNodeDiscrete;
protected:
    // Static attributes
    LocalProblem *owner;

    std::vector<std::pair<__gnu_cxx::slist<int>,Interval> > children_state;

    // Dynamic attributes (modified during heuristic computation).
    double cost;
    inline double priority() const;
    bool expanded;

    waiting_discrete_t waiting_list_discrete;
    waiting_comp_t waiting_list_comp;

public:
    virtual ~LocalProblemNode() {
    }
    LocalProblemNode(LocalProblem* owner, int children_state_size) :
	owner(owner) {
	__gnu_cxx::slist<int> s;
        Interval i = Interval(0.0, 0.0);
        pair<__gnu_cxx::slist<int>,Interval> initial_value = pair<__gnu_cxx::slist<int>,Interval>(s, i);
        children_state.resize(children_state_size, initial_value);
	cost = -1.0;
    }
    void
	    add_to_waiting_list(LocalTransitionDiscrete *transition,
		    int start_val);
    void add_to_waiting_list(LocalTransitionComp *transition, int start_val);
    void remove_from_waiting_list(LocalTransitionDiscrete *transition);
    void remove_from_waiting_list(LocalTransitionComp *transition);
    virtual void print_name() = 0;
};

class LocalProblem {
    friend class LocalProblemNode;
    friend class LocalProblemNodeDiscrete;
    friend class LocalProblemNodeComp;
    friend class CyclicCGHeuristic;
    friend class LocalTransitionDiscrete;
    friend class LocalTransitionComp;
protected:
    enum {QUITE_A_LOT = 10000000};
    double base_priority;
    const int var_no;
    std::vector<int> *causal_graph_parents;
    hashmap *global_to_local_parents;
public:
    virtual ~LocalProblem() {
    }
    inline bool is_initialized() const;
    LocalProblem(int var_no);
};

//*****************************************************
//discrete variables
//*****************************************************
class LocalTransitionDiscrete {
    friend class CyclicCGHeuristic;
    friend class LocalProblemDiscrete;
    friend class LocalProblemNodeDiscrete;
    friend class LocalProblemNode;
    friend class LocalProblemNodeComp;
    friend class LocalTransitionComp;

    const ValueTransitionLabel *label;

    LocalProblemNodeDiscrete *source;
    LocalProblemNodeDiscrete *target;

    //TODO: merge into one vector to reduce code redundancy
    std::vector<std::vector<LocalProblemNodeDiscrete*> >
	    nodes_waiting_for_this_discrete;
    std::vector<std::vector<LocalProblemNodeComp*> > nodes_waiting_for_this_comp;

    double target_cost;
    int unreached_conditions;

    LocalTransitionDiscrete(const ValueTransitionLabel *the_label,
	    LocalProblemNodeDiscrete *the_source,
	    LocalProblemNodeDiscrete *the_target) :
	label(the_label), source(the_source), target(the_target) {
	nodes_waiting_for_this_discrete.resize(label->all_prevails.size());
    }

    void on_source_expanded(const RelaxedState &relaxed_state);
    void on_condition_reached(int cond_no, double cost);
    void try_to_fire();
    void remove_other_waiting_nodes(LocalProblemNodeDiscrete* node,
	    int prevail_no);
    //    void remove_helper(int prevail_no, int i);	
    void print_description();
};

class LocalProblemNodeDiscrete : public LocalProblemNode {
    friend class CyclicCGHeuristic;
    friend class LocalProblemDiscrete;
    friend class LocalTransitionDiscrete;
    friend class LocalProblemNodeComp;

    // Static attributes (fixed upon initialization).
    std::vector<LocalTransitionDiscrete> outgoing_transitions;

    bool expanded;

    int value;

    LocalTransitionDiscrete *reached_by;

    LocalProblemNodeDiscrete(LocalProblemDiscrete *owner,
	    int children_state_size, int value);
    void on_expand();
    void mark_helpful_transitions(const RelaxedState &relaxed_state);
    void dump();
    void print_name();
};

class LocalProblemDiscrete : public LocalProblem {
    friend class CyclicCGHeuristic;
    friend class LocalProblemNode;
    friend class LocalProblemNodeDiscrete;
    friend class LocalTransitionDiscrete;
    friend class LocalTransitionDiscreteInstance;
    friend class LocalProblemNodeComp;

    std::vector<LocalProblemNodeDiscrete> nodes;

    const int start_val;

    void build_nodes_for_variable(int var_no);
    void build_nodes_for_goal();
public:
    LocalProblemDiscrete(int var_no, int start_val);
    void initialize(double base_priority, int start_value,
	    const RelaxedState &relaxed_state);
};

//*****************************************************
//comparison case
//*****************************************************
class LocalTransitionComp {
    friend class CyclicCGHeuristic;
    friend class LocalProblemDiscrete;
    friend class LocalProblemComp;
    friend class LocalProblemNodeDiscrete;
    friend class LocalProblemNode;
    friend class LocalProblemNodeComp;

    FuncTransition *label;

    LocalProblemNodeComp *source;

    vector<bool> conds_satiesfied;

    //TODO: merge into one vector to reduce code redundancy
    std::vector<std::vector<LocalProblemNodeDiscrete*> >
	    nodes_waiting_for_this_discrete;
    std::vector<std::vector<LocalProblemNodeComp*> > nodes_waiting_for_this_comp;

    double target_cost;

    LocalTransitionComp(FuncTransition *the_label,
	    LocalProblemNodeComp *the_source) :
	label(the_label), source(the_source) {
	target_cost = 0.0;
	//TODO: is this necessary?
	nodes_waiting_for_this_discrete.resize(label->all_prevails.size());
	nodes_waiting_for_this_comp.resize(label->all_prevails.size());
	conds_satiesfied.resize(label->all_prevails.size());
    }

    void on_condition_reached(int cond_no, double cost);
    void remove_other_waiting_nodes(LocalProblemNodeDiscrete* node,
	    int prevail_no);
    void print_description();
};

class LocalProblemNodeComp : public LocalProblemNode {
    friend class CyclicCGHeuristic;
    friend class LocalProblemNode;
    friend class LocalProblemComp;
    friend class LocalTransitionComp;

    std::vector<LocalTransitionComp> outgoing_transitions;

    vector<vector<pair<LocalProblemNode*, int> > >
	    nodes_where_this_has_subscribed;

    int value;
    LocalTransitionComp *reached_by;

    bool opened;
public:
    LocalProblemNodeComp(LocalProblemComp *owner_, int children_state_size,
	    int the_value);
    bool try_to_expand(const RelaxedState &relaxed_state);
    bool is_satiesfied(int trans_index, LocalTransitionComp* trans,
	    const RelaxedState &relaxed_state);
    bool is_directly_satiesfied(const PrevailCondition &pre_cond);
    void subscribe_to_waiting_lists();
    void updateNumericVariables(LocalTransitionComp &trans,
	    vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state);
    void updatePrimitiveNumericVariable(assignment_op a_op,
	    int primitive_var_local, int influencing_var_local,
	    vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state);
    void updateNumericVariablesRec(int var,
	    vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state);
    void updateSubtermNumericVariables(int var, binary_op op, int left_var,
	    int right_var, vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state);
    int getLocalIndexOfGlobalVariable(int global_var);
    bool check_progress_of_transition(
	    vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state);
    void mark_helpful_transitions(const RelaxedState &relaxed_state);
    void dump();
    void print_name();
};

class LocalProblemComp : public LocalProblem {
    friend class CyclicCGHeuristic;
    friend class LocalProblemNode;
    friend class LocalProblemNodeComp;
    friend class LocalTransitionDiscrete;
    friend class LocalProblemNodeDiscrete;
    friend class LocalTransitionComp;

    //nodes[0] = false, nodes[1] = true
    std::vector<LocalProblemNodeComp> nodes;

    const int start_value; //0 or 1

    binary_op comp_op;

    void build_nodes_for_variable(int var_no, int the_start_value);
public:
    LocalProblemComp(int var_no, int start_val);
    void initialize(double base_priority, int start_value,
	    const RelaxedState &relaxed_state);
};

//*****************************************************
//CyclicCGHeuristic
//*****************************************************
class CyclicCGHeuristic : public Heuristic {
    friend class LocalProblemDiscrete;
    friend class LocalProblemComp;
    friend class LocalProblemNodeDiscrete;
    friend class LocalProblemNodeComp;
    friend class LocalTransitionDiscrete;

    std::vector<LocalProblemDiscrete *> local_problems_discrete;
    std::vector<LocalProblemComp *> local_problems_comp;
    std::vector<std::vector<LocalProblemDiscrete *> >
	    local_problem_discrete_index;//discrete problems
    std::vector<std::vector<LocalProblemComp *> > local_problem_comp_index;//comparison problems
    LocalProblemDiscrete *goal_problem;
    LocalProblemNodeDiscrete *goal_node;

    std::vector<std::vector<LocalProblemNode *> > buckets;
    int heap_size;

    double scaling_factor;
    bool scale;
    double K, L;
    double compute_costs(const RelaxedState &relaxed_state);
    void initialize_heap();
    void add_to_heap_helper(int new_size);
    void add_to_heap(LocalProblemNode *node);
    inline LocalProblemDiscrete *get_local_problem_discrete(int var_no,
	    int value);
    inline LocalProblemComp *get_local_problem_comp(int var_no, int value);
    inline int compute_bucket_number(double priority) {
	return ((scaling_factor < 0) ? static_cast<int>(priority)
		: static_cast<int>(scaling_factor * priority));
    }

    inline double scale_heuristic(double heuristic) {
	assert(K > 0);
	if(scale) return K * log(L*heuristic+1);
	else return heuristic;
    }
protected:
    virtual void initialize();
    virtual double compute_heuristic(const TimeStampedState &state);
public:
    CyclicCGHeuristic();
    ~CyclicCGHeuristic();
    virtual bool dead_ends_are_reliable() {
	return false;
    }
};

//*****************************************************
//inline functions
//*****************************************************
inline double LocalProblemNode::priority() const {
    return cost + owner->base_priority;
}

inline bool LocalProblem::is_initialized() const {
    return base_priority != -1;
}

inline LocalProblemDiscrete *CyclicCGHeuristic::get_local_problem_discrete(
	int var_no, int value) {
    LocalProblemDiscrete *result = local_problem_discrete_index[var_no][value];
    if(!result) {
	result = new LocalProblemDiscrete(var_no,value);
	local_problem_discrete_index[var_no][value] = result;
	local_problems_discrete.push_back(result);
    }
    return result;
}

inline LocalProblemComp *CyclicCGHeuristic::get_local_problem_comp(int var_no,
	int value) {
    assert(value == 0 || value == 1);
    LocalProblemComp *result = local_problem_comp_index[var_no][value];
    if(!result) {
	result = new LocalProblemComp(var_no,value);
	local_problem_comp_index[var_no][value] = result;
	local_problems_comp.push_back(result);
    }
    return result;
}

#endif
