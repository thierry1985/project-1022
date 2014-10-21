#include "cyclic_cg_heuristic.h"
#include "domain_transition_graph.h"
//#include "globals.h"
//#include "operator.h"
#include "state.h"
#include "relaxed_state.h"

#include <algorithm>
#include <cassert>
#include <vector>
#include <set>

class CausalGraph;
using namespace std;

#define HEURISTIC_VALUE_OF_INITIAL_STATE 1000
#define LOWER_THRESHOLD_FOR_SCALING   100
#define UPPER_THRESHOLD_FOR_SCALING   100000

CyclicCGHeuristic *g_HACK = 0;

LocalProblem::LocalProblem(int the_var_no) :
    base_priority(-1.0), var_no(the_var_no), causal_graph_parents(NULL) {
}

void LocalTransitionDiscrete::on_source_expanded(
	const RelaxedState &relaxed_state) {
    /* Called when the source of this transition is reached by
     Dijkstra exploration. Tries to compute cost for the target of
     the transition from the source cost, action cost, and set-up
     costs for the conditions on the label. The latter may yet be
     unknown, in which case we "subscribe" to the waiting list of
     the node that will tell us the correct value. */

    assert(source->cost >= 0);
    assert(source->cost < LocalProblem::QUITE_A_LOT);

    assert(source->owner == target->owner);

    double duration = 0;
    if(label->duration_variable != -1) {
	//duration variable must have exactly 1 value
	assert(double_equals(relaxed_state.getIntervalOfVariable(label->duration_variable).left_point,
		relaxed_state.getIntervalOfVariable(label->duration_variable).right_point));
	duration = relaxed_state.getIntervalOfVariable(label->duration_variable).left_point;
    }

    target_cost = source->cost + duration;

    if(target->cost <= target_cost) {
	// Transition cannot find a shorter path to target.
	return;
    }

    unreached_conditions = 0;
    const vector<PrevailCondition> &prevail = label->all_prevails;

    for(int i = 0; i < prevail.size(); i++) {
	int local_var = prevail[i].local_var;
	int prev_var_no = prevail[i].prev_dtg->var;

	set<LocalProblemNodeDiscrete*> nodes_to_subscribe_discrete;
	set<LocalProblemNodeComp*> problems_to_subscribe_comp;

	if(g_variable_types[prev_var_no]==logical) {
	    int prev_value = static_cast<int>(prevail[i].value);
	    __gnu_cxx::slist<int> current_vals = source->children_state[local_var].first;
	    __gnu_cxx::slist<int>::iterator it;
	    for(it = current_vals.begin(); it != current_vals.end(); ++it) {
		LocalProblemDiscrete *child_problem =
			g_HACK->get_local_problem_discrete(prev_var_no, *it);
		if(!child_problem->is_initialized()) {
		    double base_priority = source->priority();
		    int start_value = *it;
		    child_problem->initialize(base_priority, start_value,
			    relaxed_state);
		}

		LocalProblemNodeDiscrete *cond_node =
			&child_problem->nodes[prev_value];

		if(cond_node->expanded) {
		    //factor to punish the increase of the number of values in the relaxed state
		    //		    relaxed_state.dump();
		    double number_of_current_values =
			    source->children_state[local_var].first.size();
		    double number_of_possible_values =
			    g_variable_domain[prev_var_no];
		    double punish_increasing_of_range = 1.0
			    + (number_of_current_values
				    / number_of_possible_values);

		    target_cost = target_cost + (cond_node->cost
			    * punish_increasing_of_range);
		    //		    target_cost = target_cost + cond_node->cost;
		    if(target->cost <= target_cost) {
			target_cost = target_cost - (cond_node->cost
				* punish_increasing_of_range);
			//target_cost = target_cost - cond_node->cost;
			continue;
		    } else {
			break;
		    }
		} else {
		    nodes_to_subscribe_discrete.insert(cond_node);
		}
	    }
	    if((it == current_vals.end())
		    && (nodes_to_subscribe_discrete.size()> 0)) {
		unreached_conditions++;
		set<LocalProblemNodeDiscrete*>::iterator it;
		for(it = nodes_to_subscribe_discrete.begin(); it
			!= nodes_to_subscribe_discrete.end(); ++it) {
		    (*it)->add_to_waiting_list(this, i);
		    nodes_waiting_for_this_discrete[i].push_back((*it));
		}
#ifdef DEBUG
		cout << "After expanding trans ";
		print_description();
		cout << " (" << unreached_conditions << " unreached conds)"
			<< endl;
#endif
		//		for(it = nodes_to_subscribe.begin(); it != nodes_to_subscribe.end(); ++it) {
		//		    cout << "LocalProblem " << prev_var_no << ", ";
		//		    (*it)->dump();
		//		}
	    }
	} else if(g_variable_types[prev_var_no]==primitive_functional) {
	    cout << "primitive_funct" << endl;
	    assert(false);
	} else if(g_variable_types[prev_var_no]==subterm_functional) {
	    cout << "subterm" << endl;
	    assert(false);
	} else if(g_variable_types[prev_var_no]==comparison) {
	    int prev_value = static_cast<int>(prevail[i].value);
	    __gnu_cxx::slist<int> current_vals = source->children_state[local_var].first;
	    Interval start_interval = source->children_state[0].second;
	    assert(current_vals.size() == 1 || current_vals.size() == 2);
	    if(current_vals.size() == 2) {
		// if something has to be true or false and it is already true and false, everything is fine
	    } else {
		assert(current_vals.size() == 1);
		__gnu_cxx::slist<int>::iterator it = current_vals.begin();
		int start_value = *it;
		if(start_value == prev_value) {
		    // to change nothing costs nothing
		} else {
		    LocalProblemComp *child_problem =
			    g_HACK->get_local_problem_comp(prev_var_no,
				    start_value);
#ifndef NDEBUG
		    //The variable which has to be compared with 0 is the first parent!
		    DomainTransitionGraph* dtg = prevail[i].prev_dtg;
		    DomainTransitionGraphComp* cdtg = dynamic_cast<DomainTransitionGraphComp*>(dtg);
		    if(!((*child_problem->causal_graph_parents)[0] == cdtg->nodes.first.left_var)) {
			cout << (*source->owner->causal_graph_parents)[0] << endl;
			cout << cdtg->nodes.first.left_var << endl;
			assert(false);
		    }
#endif
		    if(!child_problem->is_initialized()) {
			double base_priority = source->priority();
			child_problem->initialize(base_priority, start_value,
				relaxed_state);
		    }

		    LocalProblemNodeComp *cond_node =
			    &child_problem->nodes[1 - start_value];

		    if(cond_node->expanded) {
			target_cost = target_cost + cond_node->cost;
			if(target->cost <= target_cost) {
			    // Transition cannot find a shorter path to target.
			    return;
			}
		    } else {
			LocalProblemNodeComp *start_node =
				&child_problem->nodes[start_value];

			if(start_node->try_to_expand(relaxed_state)) {
			    target_cost = target_cost + cond_node->cost;
			    if(target->cost <= target_cost) {
				// Transition cannot find a shorter path to target.
				return;
			    }
			} else {
			    unreached_conditions++;
			    cond_node->add_to_waiting_list(this, i);
			}
		    }
		}
	    }
	}
    }
    try_to_fire();
}

void LocalTransitionDiscrete::on_condition_reached(int cond_no, double cost) {
#ifdef DEBUG
    cout << "DEBUG: Condition " << cond_no << " of ";
    print_description();
    cout << " (" << this << ") reached (all in all " << unreached_conditions
	    << " unreached conditions)." << endl;
#endif
    assert(unreached_conditions);
    --unreached_conditions;

    //factor to punish the increase of the number of values in the relaxed state
    int var_no = label->all_prevails[cond_no].prev_dtg->var;
    int local_var_no = label->all_prevails[cond_no].local_var;
    double number_of_current_values =
	    source->children_state[local_var_no].first.size();
    double number_of_possible_values = g_variable_domain[var_no];
    double punish_increasing_of_range = 1.0 + (number_of_current_values
	    / number_of_possible_values);

    target_cost = target_cost + (cost * punish_increasing_of_range);
    //    target_cost = target_cost + cost;

    try_to_fire();
#ifdef DEBUG
    cout << unreached_conditions << " condition(s) remaining" << endl;
#endif
}

void LocalTransitionDiscrete::try_to_fire() {
#ifdef DEBUG
    cout << "try to fire: ";
    print_description();
    cout << endl;
#endif
    if(!unreached_conditions && target_cost < target->cost) {
#ifdef DEBUG
	cout << "target->cost: " << target->cost << ", target_cost: "
		<< target_cost << endl;

	cout << "DEBUG: setting reached_by of ";
	target->print_name();
	cout << " to ";
	print_description();
	cout << endl;

	cout << "REACHED ";
	target->print_name();
	cout << " from ";
	source->print_name();
	cout << endl;
#endif
	target->cost = target_cost;
	target->reached_by = this;
	g_HACK->add_to_heap(target);

#ifdef DEBUG
	cout << " successful" << endl;
#endif
    } else {
#ifdef DEBUG
	cout << " unsuccessful:" << endl;
	if(unreached_conditions) {
	    cout << "  " << unreached_conditions << " unreached_conditions:"
		    << endl;
	    source->dump();
	    cout << "conds: " << endl;
	    for(int i = 0; i < label->all_prevails.size(); i++) {
		cout
			<< (*source->owner->causal_graph_parents)[label->all_prevails[i].local_var]
			<< ":" << label->all_prevails[i].value << " ";
	    }
	    cout << endl;
	} else {
	    cout << "  " << "target_cost: " << target_cost
		    << ", target->cost: " << target->cost << endl;
	}
#endif
    }
}

//void LocalTransitionDiscrete::remove_helper(int prevail_no, int i) {
//    nodes_waiting_for_this[prevail_no][i]->remove_from_waiting_list(this);
//}

void LocalTransitionDiscrete::remove_other_waiting_nodes(
	LocalProblemNodeDiscrete* node, int prevail_no) {
    //        static int most = 0;
    //        if(most < nodes_waiting_for_this[prevail_no].size()) {
    //    	most = nodes_waiting_for_this[prevail_no].size();
    //    	cout << nodes_waiting_for_this[prevail_no].size() << endl;
    //    
    //        }
    for(int i = 0; i< nodes_waiting_for_this_discrete[prevail_no].size(); i++) {
	if(nodes_waiting_for_this_discrete[prevail_no][i] != node) {
	    nodes_waiting_for_this_discrete[prevail_no][i]->remove_from_waiting_list(this);
	    //	    remove_helper(prevail_no, i);
	}
    }
    nodes_waiting_for_this_discrete[prevail_no].clear();
}

void LocalTransitionDiscrete::print_description() {
    cout << "<" << source->owner->var_no << "|" << source->value << ","
	    << target->value << "> (" << this << "), prevails: ";
    for(int i = 0; i < label->all_prevails.size(); i++) {
	cout
		<< (*source->owner->causal_graph_parents)[label->all_prevails[i].local_var]
		<< ": " << label->all_prevails[i].value << " ";
    }
}

LocalProblemNodeDiscrete::LocalProblemNodeDiscrete(
	LocalProblemDiscrete *owner_, int children_state_size, int the_value) :
    LocalProblemNode(owner_, children_state_size), value(the_value) {
    expanded = false;
    reached_by = 0;
}

void LocalProblemNode::remove_from_waiting_list(
	LocalTransitionDiscrete *transition) {
#ifdef DEBUG
    cout << "DEBUG: removing ";
    transition->print_description();
    cout << " from ";
    print_name();
#endif
    //std::vector<std::pair<LocalTransitionDiscrete*,int> >::iterator it;
    for(iterator_discrete it = waiting_list_discrete.begin(); it
	    != waiting_list_discrete.end(); ++it) {
	if(it->first == transition) {
	    waiting_list_discrete.erase(it);
	    break;
	}
    }
}

void LocalProblemNodeDiscrete::on_expand() {
    expanded = true;
    // Set children state unless this was an initial node.
    if(reached_by) {
	LocalProblemNodeDiscrete *parent = reached_by->source;
	children_state = parent->children_state;
	const vector<PrevailCondition> &prevail =
		reached_by->label->all_prevails;
	for(int i = 0; i < prevail.size(); i++) {
	    int local_var = prevail[i].local_var;
	    //	    int global_var = (*(reached_by->source->owner->causal_graph_parents))[local_var];
	    if(!is_functional(local_var))
		children_state[local_var].first.push_front(static_cast<int>(prevail[i].value));
	    else {
		children_state[local_var].second.assign(Interval(prevail[i].value));
	    }
	}
	if(parent->reached_by)
	    reached_by = parent->reached_by;
    }

    //    cout << "Expanded:";
    //    print_name();
    for(iterator_discrete it = waiting_list_discrete.begin(); it
	    != waiting_list_discrete.end(); ++it) {
	it->first->on_condition_reached(it->second, cost);
    }

    for(iterator_discrete it = waiting_list_discrete.begin(); it
	    != waiting_list_discrete.end(); ++it) {
	it->first->remove_other_waiting_nodes(this, it->second);
    }
    waiting_list_discrete.clear();

    for(iterator_comp it = waiting_list_comp.begin(); it
	    != waiting_list_comp.end(); ++it) {
	it->first->on_condition_reached(it->second, cost);
    }

    for(iterator_comp it = waiting_list_comp.begin(); it
	    != waiting_list_comp.end(); ++it) {
	it->first->remove_other_waiting_nodes(this, it->second);
    }
    waiting_list_comp.clear();
}

void LocalProblemNodeDiscrete::mark_helpful_transitions(
	const RelaxedState &relaxed_state) {
    if(!(cost >= 0 && cost < LocalProblem::QUITE_A_LOT)) {
		dump();
	cout << "cost: " << cost << endl;
	assert(false);
    }
    if(reached_by) {
#ifdef DEBUG
	cout << "reached_by is: ";
	reached_by->print_description();
	cout << endl;
	assert(reached_by->label);
#endif
	double duration = 0;
	//duration must have exactly one value
	int duration_variable = reached_by->label->duration_variable;
	if(!(duration_variable == -1)) {
#ifndef NDEBUG
	    double left_point = relaxed_state.getIntervalOfVariable(duration_variable).left_point;
	    double right_point = relaxed_state.getIntervalOfVariable(duration_variable).right_point;
	    assert(double_equals(left_point,right_point));
#endif
	    duration = relaxed_state.getIntervalOfVariable(reached_by->label->duration_variable).left_point;
	}
#ifdef DEBUG
	cout << "duration of trans is: " << duration << endl;
	cout << "reached_by->target_cost is: " << reached_by->target_cost
		<< endl;
#endif
	if(double_equals(reached_by->target_cost, duration)) {
	    assert(reached_by->label);

	    // if reached_by->label->op is NULL this means that the transition corresponds to
	    // an axiom. Do not add this axiom (or rather a NULL pointer) to the set of
	    // preferred operators.
	    if(reached_by->label->op) {
#ifdef DEBUG
		cout << "mark " << reached_by->label->op->get_name()
			<< " as pref op!" << endl;
#endif

		if(reached_by->label->op == NULL) {
		    reached_by->target->print_name();
		    assert(false);
		}

		const Operator *op = reached_by->label->op;
		g_HACK->set_preferred(op);
	    }
	} else {
#ifdef DEBUG
	    cout << "no pref op. recursively continue..." << endl;
#endif
	    // Recursively compute helpful transitions for prevailed variables.
	    const vector<PrevailCondition> &prevail =
		    reached_by->label->all_prevails;
	    for(int i = 0; i < prevail.size(); i++) {
		int prev_var_no = prevail[i].prev_dtg->var;

		int prev_value = static_cast<int>(prevail[i].value);

		if(g_variable_types[prev_var_no] == logical) {
		    __gnu_cxx::slist<int> values =
			    relaxed_state.getValuesOfVariable(prev_var_no);
		    assert(!values.empty());
		    __gnu_cxx::slist<int>::iterator it;
		    //For each value of the variable in the relaxed state there is a node with the cost of
		    //the transition in dependence of the context value. We only consider the minimal one!
		    LocalProblemNodeDiscrete *child_node= NULL;
		    for(it = values.begin(); it != values.end(); ++it) {
			LocalProblemNodeDiscrete *new_child_node =
				&g_HACK->get_local_problem_discrete(prev_var_no,
					*it)->nodes[prev_value];
			if(!child_node || (new_child_node->cost
				< child_node->cost)) {
			    child_node = new_child_node;
			}
		    }
		    assert(child_node);
		    assert(child_node->cost < LocalProblem::QUITE_A_LOT);
		    if(child_node->cost < LocalProblem::QUITE_A_LOT)
			child_node->mark_helpful_transitions(relaxed_state);
		}

		if(g_variable_types[prev_var_no] == comparison) {
		    __gnu_cxx::slist<int> values =
			    relaxed_state.getValuesOfVariable(prev_var_no);
		    assert(!values.empty());
		    __gnu_cxx::slist<int>::iterator it;
		    //For each value of the variable in the relaxed state there is a node with the cost of
		    //the transition in dependence of the context value. We only consider the minimal one!
		    LocalProblemNodeComp *child_node= NULL;
		    for(it = values.begin(); it != values.end(); ++it) {
			LocalProblemNodeComp *new_child_node =
				&g_HACK->get_local_problem_comp(prev_var_no,
					*it)->nodes[prev_value];
			if(!child_node || (new_child_node->cost
				< child_node->cost)) {
			    child_node = new_child_node;
			}
		    }
		    assert(child_node);
		    assert(child_node->cost < LocalProblem::QUITE_A_LOT);
		    if(child_node->cost < LocalProblem::QUITE_A_LOT)
			child_node->mark_helpful_transitions(relaxed_state);
		}
	    }
	}
    }
#ifdef DEBUG
    cout << "reached_by is NULL" << endl;
#endif
}

void LocalProblemNodeComp::mark_helpful_transitions(
	const RelaxedState &relaxed_state) {
  if(!this->owner->is_initialized()) {
    //condition was already satiesfied!
    return;
  }
    if(reached_by) {
	double duration = 0;
	//duration must have exactly one value
	int duration_variable_local = reached_by->label->duration_variable;
	int
		duration_variable_global =
			(*reached_by->source->owner->causal_graph_parents)[duration_variable_local];
	if(!(duration_variable_global == -1)) {
#ifndef NDEBUG
	    double left_point = relaxed_state.getIntervalOfVariable(duration_variable_global).left_point;
	    double right_point = relaxed_state.getIntervalOfVariable(duration_variable_global).right_point;
	    if(!(double_equals(left_point,right_point))) {
		cout << "duration_variable_global: " << duration_variable_global << endl;
		cout << "left_point: " << left_point << endl;
		cout << "right_point: " << right_point << endl;
		dump();
		assert(false);
	    }
#endif
	    duration = relaxed_state.getIntervalOfVariable(duration_variable_global).right_point;
	}
	if(double_equals(reached_by->target_cost, duration)) {
	    assert(reached_by->label);

	    // if reached_by->label->op is NULL this means that the transition corresponds to
	    // an axiom. Do not add this axiom (or rather a NULL pointer) to the set of
	    // preferred operators.
	    if(reached_by->label->op) {
		const Operator *op = reached_by->label->op;
		g_HACK->set_preferred(op);
	    }
	} else {
	    // Recursively compute helpful transitions for prevailed variables.
	    const vector<PrevailCondition> &prevail =
		    reached_by->label->all_prevails;
	    for(int i = 0; i < prevail.size(); i++) {
		int prev_var_no = prevail[i].prev_dtg->var;

		int prev_value = static_cast<int>(prevail[i].value);

		if(g_variable_types[prev_var_no] == logical) {
		    __gnu_cxx::slist<int> values =
			    relaxed_state.getValuesOfVariable(prev_var_no);
		    assert(!values.empty());
		    __gnu_cxx::slist<int>::iterator it;
		    //For each value of the variable in the relaxed state there is a node with the cost of
		    //the transition in dependence of the context value. We only consider the minimal one!
		    LocalProblemNodeDiscrete *child_node= NULL;
		    for(it = values.begin(); it != values.end(); ++it) {
			LocalProblemNodeDiscrete *new_child_node =
				&g_HACK->get_local_problem_discrete(prev_var_no,
					*it)->nodes[prev_value];
			if(!child_node || (new_child_node->cost
				< child_node->cost)) {
			    child_node = new_child_node;
			}
		    }
		    assert(child_node);
		    assert(child_node->cost < LocalProblem::QUITE_A_LOT);
		    if(child_node->cost < LocalProblem::QUITE_A_LOT)
			child_node->mark_helpful_transitions(relaxed_state);
		}

		if(g_variable_types[prev_var_no] == comparison) {
		    __gnu_cxx::slist<int> values =
			    relaxed_state.getValuesOfVariable(prev_var_no);
		    assert(!values.empty());
		    __gnu_cxx::slist<int>::iterator it;
		    //For each value of the variable in the relaxed state there is a node with the cost of
		    //the transition in dependence of the context value. We only consider the minimal one!
		    LocalProblemNodeComp *child_node= NULL;
		    for(it = values.begin(); it != values.end(); ++it) {
			LocalProblemNodeComp *new_child_node =
				&g_HACK->get_local_problem_comp(prev_var_no,
					*it)->nodes[prev_value];
			if(!child_node || (new_child_node->cost
				< child_node->cost)) {
			    child_node = new_child_node;
			}
		    }
		    assert(child_node);
		    assert(child_node->cost < LocalProblem::QUITE_A_LOT);
		    if(child_node->cost < LocalProblem::QUITE_A_LOT)
			child_node->mark_helpful_transitions(relaxed_state);
		}
	    }
	}
    }
}

void LocalProblemNodeDiscrete::dump() {
    cout << "---------------" << endl;
    print_name();
    cout << "Discrete Waiting list:" << endl;
    for(const_iterator_discrete it = waiting_list_discrete.begin(); it
	    != waiting_list_discrete.end(); ++it) {
	cout << " ";
	it->first->print_description();
	cout << "," << it->second << endl;
    }
    cout << "Comp Waiting list:" << endl;
    for(const_iterator_comp it = waiting_list_comp.begin(); it
	    != waiting_list_comp.end(); ++it) {
	cout << " ";
	it->first->print_description();
	cout << "," << it->second << endl;
    }

    cout << "Context:" << endl;
    if(!expanded)
	cout << " not set yet!" << endl;
    else {
	for(int i = 0; i < owner->causal_graph_parents->size(); i++) {
	    cout << (*owner->causal_graph_parents)[i] << ": ";
	    if(is_functional((*owner->causal_graph_parents)[i])) {
		cout << "[" << children_state[i].second.left_point << ","
			<< children_state[i].second.right_point << "]" << endl;
	    } else {
		__gnu_cxx::slist<int> current_vals = children_state[i].first;
		__gnu_cxx::slist<int>::iterator it;
		for(it = current_vals.begin(); it != current_vals.end(); ++it) {
		    cout << *it << " ";
		}
		cout << endl;
	    }
	}
    }
    cout << "cost: " << cost << endl;
    cout << "priority: " << priority() << endl;
    cout << "---------------" << endl;
}

void LocalProblemNodeDiscrete::print_name() {
    cout << "Local Problem [" << owner->var_no << "," << (dynamic_cast<LocalProblemDiscrete*>(owner))->start_val << "], node " << value << " (" << this
	    <<")" << endl;
}

void LocalProblemDiscrete::build_nodes_for_variable(int var_no) {
    if(!(g_variable_types[var_no] == logical)) {
	cout << "variable type: " << g_variable_types[var_no] << endl;
	assert(false);
    }
    DomainTransitionGraph *dtg = g_transition_graphs[var_no];
    DomainTransitionGraphSymb *dtgs =
	    dynamic_cast<DomainTransitionGraphSymb *>(dtg);
    assert(dtgs);

    causal_graph_parents = &dtg->ccg_parents;
    global_to_local_parents = &dtg->global_to_local_ccg_parents;

    int num_parents = causal_graph_parents->size();
    for(int value = 0; value < g_variable_domain[var_no]; value++)
	nodes.push_back(LocalProblemNodeDiscrete(this, num_parents, value));

    // Compile the DTG arcs into LocalTransition objects.
    for(int value = 0; value < nodes.size(); value++) {
	LocalProblemNodeDiscrete &node = nodes[value];
	const ValueNode &dtg_node = dtgs->nodes[value];
	for(int i = 0; i < dtg_node.transitions.size(); i++) {
	    const ValueTransition &dtg_trans = dtg_node.transitions[i];
	    int target_value = dtg_trans.target->value;
	    LocalProblemNodeDiscrete &target = nodes[target_value];
	    for(int j = 0; j < dtg_trans.ccg_labels.size(); j++) {
		const ValueTransitionLabel &label = dtg_trans.ccg_labels[j];
		LocalTransitionDiscrete trans(&label, &node, &target);
		node.outgoing_transitions.push_back(trans);
	    }
	}

    }
}

void LocalProblemDiscrete::build_nodes_for_goal() {
    // TODO: We have a small memory leak here. Could be fixed by
    // making two LocalProblem classes with a virtual destructor.
    causal_graph_parents = new vector<int>;
    for(int i = 0; i < g_goal.size(); i++)
	causal_graph_parents->push_back(g_goal[i].first);

    for(int value = 0; value < 2; value++)
	nodes.push_back(LocalProblemNodeDiscrete(this, g_goal.size(), value));

    vector<PrevailCondition> goals;
    for(int i = 0; i < g_goal.size(); i++) {
	int goal_var = g_goal[i].first;
	double goal_value = g_goal[i].second;
	DomainTransitionGraph *goal_dtg = g_transition_graphs[goal_var];
	goals.push_back(PrevailCondition(goal_dtg, i, goal_value,
		PrevailCondition::discrete_var));
    }
    ValueTransitionLabel *label = new ValueTransitionLabel(-1, goals);
    LocalProblemNodeDiscrete *target = &nodes[1];
    LocalProblemNodeDiscrete *start = &nodes[0];
    assert(label);
    LocalTransitionDiscrete trans(label, start, target);

    nodes[0].outgoing_transitions.push_back(trans);
}

LocalProblemDiscrete::LocalProblemDiscrete(int the_var_no, int the_start_val) :
    LocalProblem(the_var_no), start_val(the_start_val) {
    if(var_no == -1)
	build_nodes_for_goal();
    else
	build_nodes_for_variable(var_no);
}

void LocalProblemDiscrete::initialize(double base_priority_, int start_value,
	const RelaxedState &relaxed_state) {
    assert(!is_initialized());
    base_priority = base_priority_;

    for(int to_value = 0; to_value < nodes.size(); to_value++) {
	nodes[to_value].expanded = false;
	nodes[to_value].cost = QUITE_A_LOT;
	nodes[to_value].reached_by = NULL;
	nodes[to_value].waiting_list_discrete.clear();
	nodes[to_value].waiting_list_comp.clear();
    }

    LocalProblemNodeDiscrete *start = &nodes[start_value];
    start->cost = 0;

    int parents_num = causal_graph_parents->size();
    __gnu_cxx::slist<int> s;
    Interval i = Interval(0.0, 0.0);
    pair<__gnu_cxx::slist<int>,Interval> initial_value = pair<__gnu_cxx::slist<int>, Interval>(s, i);
    start->children_state.resize(parents_num, initial_value);
    for(int i = 0; i < parents_num; i++) {
	int var = (*causal_graph_parents)[i];
	if(!is_functional(var)) {
	    start->children_state[i].first
		    = relaxed_state.getValuesOfVariable(var);
	} else {
	    start->children_state[i].second
		    = relaxed_state.getIntervalOfVariable(var);
	}
    }
    g_HACK->add_to_heap(start);
}

void LocalTransitionComp::print_description() {
    cout << "<" << source->owner->var_no << "|" << source->value << "> ("
	    << this << "), prevails: ";
    for(int i = 0; i < label->all_prevails.size(); i++) {
	cout
		<< (*source->owner->causal_graph_parents)[label->all_prevails[i].local_var]
		<< ": " << label->all_prevails[i].value << " ";
    }
}

LocalProblemNodeComp::LocalProblemNodeComp(LocalProblemComp *owner_,
	int children_state_size, int the_value) :
    LocalProblemNode(owner_, children_state_size), value(the_value) {
    expanded = false;
    opened = false;
    reached_by = NULL;
}

void LocalProblemNode::add_to_waiting_list(LocalTransitionDiscrete *transition,
	int prevail_no) {
#ifdef DEBUG
    cout << "adding ";
    transition->print_description();
    cout << "," << prevail_no;
    cout << " to waiting_list of ";
    print_name();
#endif

#ifndef NDEBUG
    for(const_iterator_discrete it = waiting_list_discrete.begin(); it != waiting_list_discrete.end(); ++it) {
	if(it->first == transition) {
	    transition->print_description();
	    cout << "," << it->second;
	    cout << " allready inserted into waiting_list of ";
	    print_name();
	    assert(false);
	}
    }
    //    for(int i = 0; i < waiting_list.size(); i++) {
    //	if(waiting_list[i].first == transition) {
    //	    transition->print_description();
    //	    cout << "," << waiting_list[i].second;
    //	    cout << " allready inserted into waiting_list of ";
    //	    print_name();
    //	    assert(false);
    //	}
    //    }
#endif

    waiting_list_discrete.push_front(make_pair(transition, prevail_no));
}

void LocalProblemNode::add_to_waiting_list(LocalTransitionComp *transition,
	int prevail_no) {
#ifdef DEBUG
    cout << "adding ";
    transition->print_description();
    cout << "," << prevail_no;
    cout << " to waiting_list of ";
    print_name();
#endif

#ifndef NDEBUG
    for(const_iterator_comp it = waiting_list_comp.begin(); it != waiting_list_comp.end(); ++it) {
	if(it->first == transition) {
	    transition->print_description();
	    cout << "," << it->second;
	    cout << " allready inserted into waiting_list of ";
	    print_name();
	    assert(false);
	}
    }
#endif

    waiting_list_comp.push_front(make_pair(transition, prevail_no));
}

bool LocalProblemNodeComp::try_to_expand(const RelaxedState &relaxed_state) {
    if(opened)
	return false;
    opened = true;
    vector<LocalTransitionComp*> ready_transitions;
    nodes_where_this_has_subscribed.resize(outgoing_transitions.size());
    vector<pair<__gnu_cxx::slist<int>,Interval> > temp_children_state(children_state);
    for(int i = 0; i < outgoing_transitions.size(); i++) {
	LocalTransitionComp *trans = &outgoing_transitions[i];
	temp_children_state = children_state;
	    updateNumericVariables((*trans), temp_children_state);
	if(check_progress_of_transition(temp_children_state)) {
	    if(is_satiesfied(i, trans, relaxed_state)) {
		ready_transitions.push_back(trans);
	    }
	}
    }
	    //	    dump();
	    //	    cout << "new_children_state:" << endl;
	    //	    for(int j = 0; j < owner->causal_graph_parents->size(); j++) {
	    //		cout << (*owner->causal_graph_parents)[j] << ": ";
	    //		if(is_functional((*owner->causal_graph_parents)[j])) {
	    //		    cout << "[" << temp_children_state[j].second.left_point
	    //			    << "," << temp_children_state[j].second.right_point
	    //			    << "]" << endl;
	    //		} else {
	    //		    set<int> current_vals = temp_children_state[j].first;
	    //		    set<int>::iterator it;
	    //		    for(it = current_vals.begin(); it != current_vals.end(); ++it) {
	    //			cout << *it << " ";
	    //		    }
	    //		    cout << endl;
	    //		}
	    //	    }
    if(!ready_transitions.empty()) {
		// this is the right transition!!!
		LocalProblemComp *prob = dynamic_cast<LocalProblemComp*>(owner);
		LocalProblemNodeComp *target_node =
			&(prob->nodes[1 - prob->start_value]);
		target_node->reached_by = ready_transitions[0];
		assert(target_node->cost == LocalProblem::QUITE_A_LOT);
		// we have to add the costs of the transition itself!
		int duration_var_local =
			ready_transitions[0]->label->duration_variable;
		// duration has to be unique!
		assert(children_state[duration_var_local].second.left_point
			== children_state[duration_var_local].second.right_point);
		double duration = children_state[duration_var_local].second.right_point;
		if(duration < 0)
			duration = 30;
		ready_transitions[0]->target_cost += duration;
		target_node->cost
			= ready_transitions[0]->target_cost;
		target_node->expanded = true;
		expanded = true;

	//call transitions on the own waiting lists
	for(iterator_discrete it = target_node->waiting_list_discrete.begin(); it
		!= target_node->waiting_list_discrete.end(); ++it) {
	    it->first->on_condition_reached(it->second, target_node->cost);
	}
	target_node->waiting_list_discrete.clear();

	for(iterator_comp it = target_node->waiting_list_comp.begin(); it
		!= target_node->waiting_list_comp.end(); ++it) {
	    it->first->on_condition_reached(it->second, target_node->cost);
	}
	target_node->waiting_list_comp.clear();

		return true;
    } else {
	subscribe_to_waiting_lists();
	return false;
    }
}

bool LocalProblemNodeComp::check_progress_of_transition(
	vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state) {
    Interval &old_values = children_state[0].second;
    Interval &new_values = temp_children_state[0].second;
    LocalProblemComp *prob = dynamic_cast<LocalProblemComp*>(owner);
    assert(prob);
    binary_op comp_op = prob->comp_op;
    switch (comp_op) {
    case lt:
    case le:
	if(new_values.left_point < old_values.left_point)
	    return true;
	else
	    return false;
    case gt:
    case ge:
	if(new_values.right_point > old_values.right_point)
	    return true;
	else
	    return false;
    case eq:
	assert(old_values.right_point < 0 || old_values.left_point> 0);
	if(old_values.right_point < 0) {
	    if(new_values.right_point> old_values.right_point)
	    return true;
	    return false;
	} else {
	    if(new_values.left_point < old_values.left_point)
	    return true;
	    return false;
	}
	case ue:
	default:
	return true;
    }
}

void LocalTransitionComp::remove_other_waiting_nodes(
	LocalProblemNodeDiscrete* node, int prevail_no) {
    for(int i = 0; i< nodes_waiting_for_this_discrete[prevail_no].size(); i++) {
	if(nodes_waiting_for_this_discrete[prevail_no][i] != node) {
	    nodes_waiting_for_this_discrete[prevail_no][i]->remove_from_waiting_list(this);
	    //	    remove_helper(prevail_no, i);
	}
    }
    nodes_waiting_for_this_discrete[prevail_no].clear();
}

void LocalTransitionComp::on_condition_reached(int cond_no, double cost) {
    if(!source->opened || source->expanded ||
    conds_satiesfied[cond_no] == true) {
	return;
    }
    target_cost += cost;
    assert(conds_satiesfied[cond_no] == false);
    conds_satiesfied[cond_no] = true;
    for(int i = 0; i < conds_satiesfied.size(); i++)
	if(conds_satiesfied[i] == false)
	    return;
    // all conditions are satiesfied!
    //vector<pair<set<int>,Interval> > temp_children_state;
    //for(int j = 0; j < source->children_state.size(); j++) {
    //	temp_children_state.push_back(source->children_state[j]);
    //}
    // source->updateNumericVariables(*this, temp_children_state);
    //if(source->check_progress_of_transition(temp_children_state)) {
	// this is the right transition!!!
	LocalProblemComp *prob = dynamic_cast<LocalProblemComp*>(source->owner);
	LocalProblemNodeComp *target_node =
		&(prob->nodes[1 - prob->start_value]);
	target_node->reached_by = this;
	assert(target_node->cost == LocalProblem::QUITE_A_LOT);
	// we have to add the costs of the transition itself!
	int duration_var_local = this->label->duration_variable;
	// duration has to be unique!
	assert(source->children_state[duration_var_local].second.left_point
		== source->children_state[duration_var_local].second.right_point);
	double duration = source->children_state[duration_var_local].second.right_point;
	if(duration < 0)
		duration = 30;
	target_cost += duration;
	target_node->cost
		= target_cost;
	target_node->expanded = true;
	source->expanded = true;

	//call transitions on the own waiting lists
	for(iterator_discrete it = target_node->waiting_list_discrete.begin(); it
		!= target_node->waiting_list_discrete.end(); ++it) {
	    it->first->on_condition_reached(it->second, target_node->cost);
	}
	target_node->waiting_list_discrete.clear();

	for(iterator_comp it = target_node->waiting_list_comp.begin(); it
		!= target_node->waiting_list_comp.end(); ++it) {
	    it->first->on_condition_reached(it->second, target_node->cost);
	}
	target_node->waiting_list_comp.clear();

	//	//unsubscribe from other waiting nodes
	//	for(int i = 0; i < source->nodes_where_this_has_subscribed.size(); i++) {
	//	    for(int j = 0; j
	//		    < source->nodes_where_this_has_subscribed[i].size(); j++) {
	//		source->nodes_where_this_has_subscribed[i][j].first->remove_from_waiting_list(this);
	//	    }
	//	}
	//}

}

bool LocalProblemNodeComp::is_satiesfied(int trans_index,
	LocalTransitionComp* trans, const RelaxedState &relaxed_state) {
//    for(int i = 0; i < trans->conds_satiesfied.size(); i++) {
//	cout << trans->conds_satiesfied[i] << ",";
//    }
    bool ret = true;
    for(int i = 0; i < trans->label->all_prevails.size(); i++) {
	const PrevailCondition &pre_cond = trans->label->all_prevails[i];
	if(is_directly_satiesfied(pre_cond)) {
	    assert(trans->conds_satiesfied[i] == false);
	    trans->conds_satiesfied[i] = true;
	} else {
	    // check whether the cost of this prevail has already been computed
	    int global_var = (*(owner->causal_graph_parents))[pre_cond.local_var];
	    if(is_functional(global_var)) {
		cout << "local_var: " << pre_cond.local_var << endl;
		cout << "global_var: " << global_var << endl;
		dump();
		assert(false);
	    }
	    __gnu_cxx::slist<int> current_vals = children_state[pre_cond.local_var].first;
	    __gnu_cxx::slist<int>::iterator it;

	    if(g_variable_types[global_var] == logical) {
		for(it = current_vals.begin(); it != current_vals.end(); ++it) {
		    int start_value = *it;
		    LocalProblemDiscrete *child_problem =
			    g_HACK->get_local_problem_discrete(global_var,
				    start_value);
		    if(!child_problem->is_initialized()) {
			double base_priority = priority();
			child_problem->initialize(base_priority, start_value,
				relaxed_state);
		    }
		    int prev_value = static_cast<int>(pre_cond.value);
		    LocalProblemNodeDiscrete *cond_node =
			    &child_problem->nodes[prev_value];
		    if(cond_node->expanded) {
			trans->target_cost = trans->target_cost
				+ cond_node->cost;
			assert(trans->conds_satiesfied[i] == false);
			trans->conds_satiesfied[i] = true;
			break;
		    } else {
			nodes_where_this_has_subscribed[trans_index].push_back(make_pair(
				cond_node, i));
		    }
		}
		if(it == current_vals.end()) {
		    //the cost of this prevail has not been determined so far...
		    ret = false;
		}
	    }

	    else if(g_variable_types[global_var] == comparison) {

		if(current_vals.size() == 2) {
		    // if something has to be true or false and it is already true and false, 
		    // everything is fine.
		    assert(trans->conds_satiesfied[i] == false);
		    trans->conds_satiesfied[i] = true;
		    continue;
		}
		assert(current_vals.size() == 1);
		it = current_vals.begin();
		int start_value = *it;
		if(start_value == pre_cond.value) {
		    // to change nothing costs nothing
		    assert(trans->conds_satiesfied[i] == false);
		    trans->conds_satiesfied[i] = true;
		    continue;
		}
		LocalProblemComp *child_problem =
			g_HACK->get_local_problem_comp(global_var, start_value);
		if(!child_problem->is_initialized()) {
		}
		if(!child_problem->is_initialized()) {
		    double base_priority = priority();
		    child_problem->initialize(base_priority, start_value,
			    relaxed_state);
		}
		int prev_value = static_cast<int>(pre_cond.value);
		LocalProblemNodeComp *cond_node =
			&child_problem->nodes[prev_value];
		if(cond_node->expanded) {
		    assert(trans->conds_satiesfied[i] == false);
		    trans->conds_satiesfied[i] = true;
		    trans->target_cost = trans->target_cost + cond_node->cost;
		    break;
		} else {
		    nodes_where_this_has_subscribed[trans_index].push_back(make_pair(
			    cond_node, i));
		    ret = false;
		}
	    }

	}

    }
    return ret;
}

bool LocalProblemNodeComp::is_directly_satiesfied(
	const PrevailCondition &pre_cond) {
    int local_var = pre_cond.local_var;
    int global_var = (*(owner->causal_graph_parents))[local_var];
    if(!is_functional(global_var)) {
	__gnu_cxx::slist<int> &values = children_state[pre_cond.local_var].first;
	__gnu_cxx::slist<int>::iterator it;
	for(it = values.begin(); it != values.end(); ++it) {
	    if(*it == pre_cond.value)
		return true;
	}
    } else {
	if((children_state[pre_cond.local_var].second.left_point
		<= pre_cond.value)
		&&(children_state[pre_cond.local_var].second.right_point
			>= pre_cond.value))
	    return true;
    }
    return false;
}

void LocalProblemNodeComp::subscribe_to_waiting_lists() {
    //    cout << "nodes_to_subscribe_to:" << endl;
    for(int i = 0; i < nodes_where_this_has_subscribed.size(); i++) {
	//	cout << i << ":" << endl;
	for(int j = 0; j < nodes_where_this_has_subscribed[i].size(); j++) {
	    //	    cout << " ";
	    //	    nodes_where_this_has_subscribed[i][j].first->print_name();
	    //	    cout << " ";
	    //	    cout << nodes_where_this_has_subscribed[i][j].second;
	    //	    cout << endl;
	    LocalProblemNode *prob =
		    nodes_where_this_has_subscribed[i][j].first;
	    int prevail_no = nodes_where_this_has_subscribed[i][j].second;
	    prob->add_to_waiting_list(&(outgoing_transitions[i]), prevail_no);
	}
    }
}

void LocalProblemNodeComp::updateNumericVariables(LocalTransitionComp &trans,
	vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state) {
    int primitive_var_local = trans.label->starting_variable;
    assert(g_variable_types[(*owner->causal_graph_parents)[primitive_var_local]]
	    == primitive_functional);
    int influencing_var_local = trans.label->influencing_variable;
    assert(is_functional((*owner->causal_graph_parents)[influencing_var_local]));
    assignment_op a_op = trans.label->a_op;
    updatePrimitiveNumericVariable(a_op, primitive_var_local,
	    influencing_var_local, temp_children_state);
}

void LocalProblemNodeComp::updatePrimitiveNumericVariable(assignment_op a_op,
	int primitive_var_local, int influencing_var_local,
	vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state) {
    Interval &left_interval = temp_children_state[primitive_var_local].second;
    Interval &right_interval =
	    temp_children_state[influencing_var_local].second;
    switch (a_op) {
    case assign:
	left_interval.assign(right_interval);
	break;
    case scale_up:
	left_interval.scale_up(right_interval);
	break;
    case scale_down:
	left_interval.scale_down(right_interval);
	break;
    case increase:
	left_interval.increase(right_interval);
	break;
    case decrease:
	left_interval.decrease(right_interval);
	break;
    default:
	assert(false);
	break;
    }
    updateNumericVariablesRec(primitive_var_local, temp_children_state);
}

void LocalProblemNodeComp::updateNumericVariablesRec(int var,
	vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state) {
    vector<int> depending = g_causal_graph->get_successors((*owner->causal_graph_parents)[var]);
    for(int i = 0; i < depending.size(); i++) {
	int depending_var_local =
		getLocalIndexOfGlobalVariable(depending[i]);
	if(depending_var_local != -1) { //otherwise, the affected variable is not in the current context and therefore its change is not interessting for us....
	DomainTransitionGraph *dtg = g_transition_graphs[depending[i]];
	if(g_variable_types[depending[i]] == subterm_functional) {
	    DomainTransitionGraphSubterm *dtgs =
		    dynamic_cast<DomainTransitionGraphSubterm*>(dtg);
//	    assert(dtgs->get_left_var() == var || dtgs->get_right_var() == var);
		int left_var_global = dtgs->get_left_var();
		int left_var_local =
			getLocalIndexOfGlobalVariable(left_var_global);
		int right_var_global = dtgs->get_right_var();
		int right_var_local =
			getLocalIndexOfGlobalVariable(right_var_global);
		if(left_var_local != -1 && right_var_local != -1) {
	    updateSubtermNumericVariables(depending_var_local, dtgs->get_op(),
		    left_var_local, right_var_local,
		    temp_children_state);
		}
	}
	}
    }
}

int LocalProblemNodeComp::getLocalIndexOfGlobalVariable(int global_var) {
//    for(int i = 0; i < (*owner->causal_graph_parents).size(); i++) {
//	if((*owner->causal_graph_parents)[i]==global_var) {
//	    return i;
//	}
//    }
//    return -1;
  hashmap::const_iterator iter;
  iter = owner->global_to_local_parents->find(global_var);
  if (iter == owner->global_to_local_parents->end())
    return -1;
  return (*iter).second;
}

void LocalProblemNodeComp::updateSubtermNumericVariables(int var, binary_op op,
	int left_var, int right_var,
	vector<pair<__gnu_cxx::slist<int>,Interval> > &temp_children_state) {
    Interval &left = temp_children_state[left_var].second;
    Interval &right = temp_children_state[right_var].second;
    Interval &target = temp_children_state[var].second;
    switch (op) {
    case add:
	target = intersect(target, add_intervals(left, right));
	break;
    case subtract:
	target = intersect(target, subtract_intervals(left, right));
	break;
    case mult:
	target = intersect(target, multiply_intervals(left, right));
	break;
    case divis:
	target = intersect(target, divide_intervals(left, right));
	break;
    default:
	assert(false);
	break;
    }
    updateNumericVariablesRec(var, temp_children_state);
}

void LocalProblemNode::remove_from_waiting_list(LocalTransitionComp *transition) {
#ifdef DEBUG
    cout << "DEBUG: removing ";
    transition->print_description();
    cout << " from ";
    print_name();
#endif
    //std::vector<std::pair<LocalTransitionDiscrete*,int> >::iterator it;
    for(iterator_comp it = waiting_list_comp.begin(); it
	    != waiting_list_comp.end(); ++it) {
	if(it->first == transition) {
	    waiting_list_comp.erase(it);
	    break;
	}
    }
}

void LocalProblemNodeComp::dump() {
    cout << "---------------" << endl;
    print_name();
    cout << "Discrete Waiting list:" << endl;
    for(const_iterator_discrete it = waiting_list_discrete.begin(); it
	    != waiting_list_discrete.end(); ++it) {
	cout << " ";
	it->first->print_description();
	cout << "," << it->second << endl;
    }
    cout << "Comp Waiting list:" << endl;
    for(const_iterator_comp it = waiting_list_comp.begin(); it
	    != waiting_list_comp.end(); ++it) {
	cout << " ";
	it->first->print_description();
	cout << "," << it->second << endl;
    }

    cout << "Context:" << endl;
    if(!opened)
	cout << " not set yet!" << endl;
    else {
	for(int i = 0; i < owner->causal_graph_parents->size(); i++) {
	    cout << (*owner->causal_graph_parents)[i] << ": ";
	    if(is_functional((*owner->causal_graph_parents)[i])) {
		cout << "[" << children_state[i].second.left_point << ","
			<< children_state[i].second.right_point << "]" << endl;
	    } else {
		__gnu_cxx::slist<int> current_vals = children_state[i].first;
		__gnu_cxx::slist<int>::iterator it;
		for(it = current_vals.begin(); it != current_vals.end(); ++it) {
		    cout << *it << " ";
		}
		cout << endl;
	    }
	}
    }
    cout << "cost: " << cost << endl;
    cout << "priority: " << priority() << endl;
    cout << "---------------" << endl;
}

void LocalProblemNodeComp::print_name() {
    cout << "Local Problem [" << owner->var_no << "," << (dynamic_cast<LocalProblemComp*>(owner))->start_value << "], node " << value << " (" << this
	    <<")" << endl;
}

LocalProblemComp::LocalProblemComp(int the_var_no, int the_start_value) :
    LocalProblem(the_var_no), start_value(the_start_value) {
    assert(var_no >= 0);
    build_nodes_for_variable(var_no, the_start_value);
}

void LocalProblemComp::build_nodes_for_variable(int var_no, int the_start_value) {
    if(!(g_variable_types[var_no] == comparison)) {
	cout << "variable type: " << g_variable_types[var_no] << endl;
	assert(false);
    }
    DomainTransitionGraph *dtg = g_transition_graphs[var_no];
    DomainTransitionGraphComp *dtgc =
	    dynamic_cast<DomainTransitionGraphComp *>(dtg);
    assert(dtgc);

    comp_op = dtgc->nodes.first.op;

    causal_graph_parents = &dtgc->ccg_parents;
    global_to_local_parents = &dtgc->global_to_local_ccg_parents;

    int num_parents = causal_graph_parents->size();

    
    assert(g_variable_domain[var_no] == 3);
    // There are 3 values for a comp variable: false, true and undefined. In the heuristic we only have
    // to deal with the first both of them.
    for(int value = 0; value < 2; value++)
	nodes.push_back(LocalProblemNodeComp(this, num_parents, value));

    // Compile the DTG arcs into LocalTransition objects.
    for(int i = 0; i < dtgc->transitions.size(); i++) {
	LocalTransitionComp trans(&(dtgc->transitions[i]),
		&(nodes[the_start_value]));
	nodes[the_start_value].outgoing_transitions.push_back(trans);
    }
}

void LocalProblemComp::initialize(double base_priority_, int start_value,
	const RelaxedState &relaxed_state) {

    assert(start_value == this->start_value);

    int target_value = abs(start_value - 1);

    assert(!is_initialized());
    base_priority = base_priority_;

    assert(nodes.size() == 2);

    for(int to_value = 0; to_value < nodes.size(); to_value++) {
	nodes[to_value].expanded = false;
	nodes[to_value].opened = false;
	nodes[to_value].reached_by = NULL;
	nodes[to_value].waiting_list_discrete.clear();
	nodes[to_value].waiting_list_comp.clear();
	nodes[to_value].nodes_where_this_has_subscribed.clear();
    }

    nodes[start_value].cost = 0;
    nodes[target_value].cost = LocalProblem::QUITE_A_LOT;

    for(int i = 0; i < nodes[start_value].outgoing_transitions.size(); i++) {
	LocalTransitionComp &trans = nodes[start_value].outgoing_transitions[i];
	trans.target_cost = 0;
	for(int j = 0; j < trans.label->all_prevails.size(); j++) {
	    trans.conds_satiesfied[j] = false;
	}
    }

    int parents_num = causal_graph_parents->size();
    __gnu_cxx::slist<int> s;
    Interval i = Interval(0.0, 0.0);
    pair<__gnu_cxx::slist<int>,Interval> initial_value = pair<__gnu_cxx::slist<int>, Interval>(s, i);
    nodes[start_value].children_state.resize(parents_num, initial_value);
    for(int i = 0; i < parents_num; i++) {
	int var = (*causal_graph_parents)[i];
	if(!is_functional(var)) {
	    nodes[start_value].children_state[i].first
		    = relaxed_state.getValuesOfVariable(var);
	} else {
	    nodes[start_value].children_state[i].second
		    = relaxed_state.getIntervalOfVariable(var);
	}
    }
    g_HACK->add_to_heap(&nodes[start_value]);
}

CyclicCGHeuristic::CyclicCGHeuristic() {
    assert(!g_HACK);
    g_HACK = this;
    goal_problem = 0;
    goal_node = 0;
    heap_size = -1;
    scaling_factor = -1.0;
    K = -1.0;
    L = 0.01;
}

CyclicCGHeuristic::~CyclicCGHeuristic() {
    delete goal_problem;
    for(int i = 0; i < local_problems_discrete.size(); i++)
	delete local_problems_discrete[i];
    for(int i = 0; i < local_problems_comp.size(); i++)
	delete local_problems_comp[i];
}

void CyclicCGHeuristic::initialize() {
    assert(goal_problem == 0);
    cout << "Initializing cyclic causal graph heuristic...";

    int num_variables = g_variable_domain.size();

    goal_problem = new LocalProblemDiscrete(-1,0);
    goal_node = &goal_problem->nodes[1];

    local_problem_discrete_index.resize(num_variables);
    for(int var_no = 0; var_no < num_variables; var_no++) {
	int num_values = g_variable_domain[var_no];
	if(num_values> -1)
	    local_problem_discrete_index[var_no].resize(num_values, NULL);
	else
	    local_problem_discrete_index[var_no].resize(0, NULL);
    }

    local_problem_comp_index.resize(num_variables);
    for(int var_no = 0; var_no < num_variables; var_no++) {
	if(g_variable_types[var_no] == comparison)
	    local_problem_comp_index[var_no].resize(2, NULL);
	else
	    local_problem_comp_index[var_no].resize(0, NULL);
    }

    cout << "done." << endl;
}

double CyclicCGHeuristic::compute_heuristic(const TimeStampedState &state) {
    //    cout << "Starting heuristic computation. TimeStamp: " << state.get_timestamp() << endl;
    if(state.satisfies(g_goal) && state.operators.empty())
	return 0.0;
    RelaxedState relaxed_state = RelaxedState(state);
//    cout << "State: " << endl;
//    state.dump();
//    cout << "Relaxed state: " << endl;
//    relaxed_state.dump();
    initialize_heap();
    goal_problem->base_priority = -1;
    for(int i = 0; i < local_problems_discrete.size(); i++)
	local_problems_discrete[i]->base_priority = -1;
    for(int i = 0; i < local_problems_comp.size(); i++)
	local_problems_comp[i]->base_priority = -1;

    goal_problem->initialize(0.0, 0, relaxed_state);

    double heuristic = compute_costs(relaxed_state);

    if(scaling_factor < 0) {
	scaling_factor = HEURISTIC_VALUE_OF_INITIAL_STATE/heuristic;
	scale = heuristic < LOWER_THRESHOLD_FOR_SCALING || heuristic > UPPER_THRESHOLD_FOR_SCALING;
	if(heuristic < LOWER_THRESHOLD_FOR_SCALING) K =  HEURISTIC_VALUE_OF_INITIAL_STATE/log(L*heuristic+1);
	else K = 1000.0;
    }

    //    cout << "DEBUG: heuristic computation finished." << endl;
    //    cout << "h: " << heuristic << endl;

    if(heuristic != DEAD_END && heuristic != 0) {
	//	cout << "Marking helpful transitions for ";
	//	goal_node->print_name();
	goal_node->mark_helpful_transitions(relaxed_state);
    }

    if(!state.operators.empty() && (heuristic == 0))
	heuristic += 1;

    if(double_equals(heuristic, -1.0))
	return heuristic;
    return scale_heuristic(heuristic);
}

void CyclicCGHeuristic::initialize_heap() {
    //    int last_buckets_size = buckets.size();
    buckets.clear();
    //    buckets.resize(0.5 * last_buckets_size);
    heap_size = 0;
}

void CyclicCGHeuristic::add_to_heap_helper(int new_size) {
    buckets.resize(new_size);
}

void CyclicCGHeuristic::add_to_heap(LocalProblemNode *node) {
#ifdef DEBUG
    cout << "adding to heap: ";
    node->print_name();
#endif
    //TODO: In order to use the bucket implementation of a stack, we have to cast
    //the priority, which is a double value, to an int. Check whether this is passable.
    int bucket_no = compute_bucket_number(node->priority());
    //    cout << "  with priority " << bucket_no << endl;
    if(bucket_no >= buckets.size()) {
	int new_size = max<size_t>(bucket_no + 1, 2 * buckets.size());
	add_to_heap_helper(new_size);
	//	buckets.resize(new_size);
	//	cout << "DEBUG: resizing to " << new_size << endl;
    }
    buckets[bucket_no].push_back(node);
    ++heap_size;
#ifdef DEBUGEXTREM
    cout << "heap_size: " << heap_size << endl;
    cout << "current heap: " << endl;
    for(int i = 0; i < buckets.size(); i++) {
	cout << i << ": ";
	for(int j = 0; j < buckets[i].size(); j++) {
	    cout << buckets[i][j]->owner->var_no << ",";
	}
	cout << endl;
    }
#endif
}

double CyclicCGHeuristic::compute_costs(const RelaxedState &relaxed_state) {
    for(int curr_priority = 0; heap_size != 0; curr_priority++) {
	if(curr_priority >= buckets.size()) {
	    //	    cout << "DEBUG: curr_priority: " << curr_priority << endl;
	    //	    cout << "DEBUG: buckets.size(): " << buckets.size();
	    //	    cout << "current buckets: " << endl;
	    assert(false);
	}
	for(int pos = 0; pos < buckets[curr_priority].size(); pos++) {
	    //	    for(int i = 0; i < buckets.size(); i++) {
	    //		cout << i << ": ";
	    //		for(int j = 0; j < buckets[i].size(); j++) {
	    //		    cout << buckets[i][j]->owner->var_no << ",";
	    //		}
	    //		cout << endl;
	    //	    }
	    //	    cout << "removing from heap " << curr_priority << ", " << pos
	    //		    << endl;
	    LocalProblemNode *node = buckets[curr_priority][pos];
	    if((node->owner->var_no == -1)
		    || (g_variable_types[node->owner->var_no] == logical)) {
		//var_no == -1 if goal_problem
		LocalProblemNodeDiscrete *node_discrete =
			dynamic_cast<LocalProblemNodeDiscrete*>(node);
		assert(node_discrete);
		assert(node_discrete->owner->is_initialized());
#ifdef DEBUG
		cout << "removing from heap: ";
		node_discrete->print_name();
#endif
		//		cout << "current heap: " << endl;
		//		for(int i = 0; i < buckets.size(); i++) {
		//		    cout << i << ": ";
		//		    for(int j = 0; j < buckets[i].size(); j++) {
		//			cout << buckets[i][j]->owner->var_no << ",";
		//		    }
		//		    cout << endl;
		//		}
#ifdef DEBUG
		cout << "curr_priority: " << curr_priority << endl;
		cout << "node priority: " << node_discrete->priority() << endl;
#endif

		int bucket_number = compute_bucket_number(node->priority());

		if(bucket_number < curr_priority)
		    continue;
		if(node_discrete == goal_node)
		    return node_discrete->cost;

		if(bucket_number != curr_priority) {
		    cout << "bucket_number: " << bucket_number << endl;
		    cout << "node_discrete->priority(): "
			    << node_discrete->priority()<< endl;
		    cout << "curr_priority: " << curr_priority << endl;
		    for(int i = 0; i < buckets.size(); i++) {
			cout << i << ": ";
			for(int j = 0; j < buckets[i].size(); j++) {
			    cout << buckets[i][j]->owner->var_no << ",";
			}
			cout << endl;
		    }
		    assert(false);
		}
		if(!(node_discrete->expanded)) {
		    node_discrete->on_expand();
#ifdef DEBUG
		    cout
			    << "Call on_source_expanded for outgoing_transitions: "
			    << endl;
		    for(int i = 0; i
			    < node_discrete->outgoing_transitions.size(); i++) {
			cout << "  ";
			node_discrete->outgoing_transitions[i].print_description();
			cout << endl;
		    }
#endif
		    for(int i = 0; i
			    < node_discrete->outgoing_transitions.size(); i++)
			node_discrete->outgoing_transitions[i].on_source_expanded(relaxed_state);
		}
	    } else if(g_variable_types[node->owner->var_no] == comparison) {
		LocalProblemNodeComp *node_comp =
			dynamic_cast<LocalProblemNodeComp*>(node);
		assert(node_comp);
		assert(node_comp->owner->is_initialized());
		//		node_comp->print_name();
		//		cout << "current heap: " << endl;
		//		for(int i = 0; i < buckets.size(); i++) {
		//		    cout << i << ": ";
		//		    for(int j = 0; j < buckets[i].size(); j++) {
		//			cout << buckets[i][j]->owner->var_no << ",";
		//		    }
		//		    cout << endl;
		//		}
		//		cout << "curr_priority: " << curr_priority << endl;
		//		cout << "node priority: " << node_comp->priority() << endl;
		if(node_comp->priority() < curr_priority)
		    continue;
		if(!(node_comp->expanded)) {
		    node_comp->try_to_expand(relaxed_state);
		    //		    node_comp->on_expand();
		    //		    cout
		    //			    << "Call on_source_expanded for outgoing_transitions: "
		    //			    << endl;
		    //		    for(int i = 0; i
		    //			    < node_comp->outgoing_transitions.size(); i++) {
		    //			cout << "  ";
		    //			node_comp->outgoing_transitions[i].print_description();
		    //			cout << endl;
		    //		    }
		    //		    for(int i = 0; i
		    //			    < node_discrete->outgoing_transitions.size(); i++)
		    //			node_discrete->outgoing_transitions[i].on_source_expanded(relaxed_state);
		}
	    } else
		assert(false);
	}
	heap_size -= buckets[curr_priority].size();
	buckets[curr_priority].clear();
    }
//    		cout << "current heap: " << endl;
//    				for(int i = 0; i < buckets.size(); i++) {
//    				    cout << i << ": ";
//    				    for(int j = 0; j < buckets[i].size(); j++) {
//    					cout << buckets[i][j]->owner->var_no << ",";
//    				    }
//    				    cout << endl;
//    				}
//    				exit(1);
    return DEAD_END;
}
