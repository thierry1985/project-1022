#include <iostream>
#include <map>
#include <cassert>
#include <ext/hash_map>
using namespace std;
using namespace __gnu_cxx;

#include "domain_transition_graph.h"
#include "globals.h"

void DomainTransitionGraph::read_all(istream &in) {
    int var_count = g_variable_domain.size();

    // First step: Allocate graphs and nodes.
    g_transition_graphs.reserve(var_count);
    for(int var = 0; var < var_count; var++) {
	switch (g_variable_types[var]) {
	case logical: {
	    int range = g_variable_domain[var];
	    assert(range> 0);
	    DomainTransitionGraphSymb *dtg = new DomainTransitionGraphSymb(var, range);
	    g_transition_graphs.push_back(dtg);
	    break;
	}
	case primitive_functional: {
	    DomainTransitionGraphFunc *dtg = new DomainTransitionGraphFunc(var);
	    g_transition_graphs.push_back(dtg);
	    break;
	}
	case subterm_functional: {
	    DomainTransitionGraphSubterm *dtg =
		    new DomainTransitionGraphSubterm(var);
	    g_transition_graphs.push_back(dtg);
	    break;
	}
	case comparison: {
	    DomainTransitionGraphComp *dtg = new DomainTransitionGraphComp(var);
	    g_transition_graphs.push_back(dtg);
	    break;
	}
	default:
	    assert(false);
	    break;
	}
    }

    // Second step: Read transitions from file.
    for(int var = 0; var < var_count; var++)
	g_transition_graphs[var]->read_data(in);

    // Third step: Each primitive and subterm variable connected by a direct path to a comparison variable
    // has to be a direct parent.
    // Furthermore, collect all FuncTransitions of each subterm and primitive variable of a comparison
    // variable directly in the corresponding dtg.
    for(int var = 0; var < var_count; var++) {
	if(g_variable_types[var] == comparison) {
	    map<int, int> global_to_ccg_parent;
	    DomainTransitionGraph::compute_causal_graph_parents_comp(var,
		    global_to_ccg_parent);
	    DomainTransitionGraph::collect_func_transitions(var,
		    global_to_ccg_parent);
	}
    }
}

void DomainTransitionGraph::compute_causal_graph_parents_comp(int var,
	map<int, int> &global_to_ccg_parent) {
    DomainTransitionGraph *dtg = g_transition_graphs[var];
    DomainTransitionGraphComp *cdtg =
	    dynamic_cast<DomainTransitionGraphComp*>(dtg);
    assert(cdtg);
    cdtg->compute_recursively_parents(cdtg->nodes.first.left_var,
	    global_to_ccg_parent);
    cdtg->compute_recursively_parents(cdtg->nodes.first.right_var,
	    global_to_ccg_parent);
}

void DomainTransitionGraph::collect_func_transitions(int var,
	map<int, int> &global_to_ccg_parent) {
    DomainTransitionGraph *dtg = g_transition_graphs[var];
    DomainTransitionGraphComp *cdtg =
	    dynamic_cast<DomainTransitionGraphComp*>(dtg);
    assert(cdtg);
    cdtg->collect_recursively_func_transitions(cdtg->nodes.first.left_var,
	    global_to_ccg_parent);
    cdtg->collect_recursively_func_transitions(cdtg->nodes.first.right_var,
	    global_to_ccg_parent);

}

DomainTransitionGraphSymb::DomainTransitionGraphSymb(int var_index,
	int node_count) {
    is_axiom = g_axiom_layers[var_index] != -1;
    var = var_index;
    nodes.reserve(node_count);
    for(int value = 0; value < node_count; value++)
	nodes.push_back(ValueNode(this, value));
    last_helpful_transition_extraction_time = -1;
}

void DomainTransitionGraphSymb::read_data(istream &in) {

    check_magic(in, "begin_DTG");

    map<int, int> global_to_ccg_parent;
    map<pair<int, int>, int> transition_index;
    // TODO: This transition index business is caused by the fact
    //       that transitions in the input are not grouped by target
    //       like they should be. Change this.

    for(int origin = 0; origin < nodes.size(); origin++) {
	int trans_count;
	in >> trans_count;
	for(int i = 0; i < trans_count; i++) {
	    trans_type tt;
	    int target, operator_index;
	    in >> target;
	    in >> operator_index;
	    in >> tt;

	    pair<int, int> arc = make_pair(origin, target);
	    if(!transition_index.count(arc)) {
		transition_index[arc] = nodes[origin].transitions.size();
		nodes[origin].transitions.push_back(ValueTransition(&nodes[target]));
		//if assertions fails, range is to small.
		assert(double_equals(
			target,
			nodes[origin].transitions[transition_index[arc]].target->value));
	    }

	    assert(transition_index.count(arc));
	    ValueTransition *transition =
		    &nodes[origin].transitions[transition_index[arc]];

	    binary_op op;
	    int duration_variable;

	    if(tt == start || tt == end)
		in >> op >> duration_variable;
	    else {
		op = eq;
		duration_variable = -1;
	    }

	    if(op != eq) {
		cout << "Error: The duration constraint must be of the form\n";
		cout << "       (= ?duration (arithmetic_term))" << endl;
		exit(1);
	    }

	    vector<PrevailCondition> all_prevails;
	    int prevail_count;
	    in >> prevail_count;
	    for(int j = 0; j < prevail_count; j++) {
		int global_var, val;
		in >> global_var >> val;

		// Processing for full DTG (cyclic CG).
		if(!global_to_ccg_parent.count(global_var)) {
		    int local_var = ccg_parents.size();
		    global_to_ccg_parent[global_var] = ccg_parents.size();
		    ccg_parents.push_back(global_var);
		    global_to_local_ccg_parents.insert(ValuePair(global_var, local_var));
		}
		int ccg_parent = global_to_ccg_parent[global_var];
		DomainTransitionGraph *prev_dtg =
			g_transition_graphs[global_var];
		if(!is_functional(global_var))
		    all_prevails.push_back(PrevailCondition(prev_dtg,
			    ccg_parent, val, PrevailCondition::discrete_var));
		else
		    all_prevails.push_back(PrevailCondition(prev_dtg,
			    ccg_parent, val, PrevailCondition::numerical_var));
	    }
	    Operator *the_operator= NULL;

	    // FIXME: Skipping axioms for now, just to make the code compile.
	    //        Take axioms into account again asap.

	    if(is_axiom) {
		// assert(operator_index >= 0 && operator_index < g_axioms.size());
		// the_operator = &g_axioms[operator_index];
	    } else {
		assert(operator_index >= 0 && operator_index
			< g_operators.size());
		the_operator = &g_operators[operator_index];
	    }
	    transition->labels.push_back(ValueTransitionLabel(
		    duration_variable, the_operator, all_prevails));
	    transition->ccg_labels.push_back(ValueTransitionLabel(
		    duration_variable, the_operator, all_prevails));
	}
    }
    check_magic(in, "end_DTG");
}

void DomainTransitionGraphSymb::dump() const {
    cout << " (logical)" << endl;
    for(int i = 0; i < nodes.size(); i++) {
	cout << nodes[i].value << endl;
	vector<ValueTransition> transitions = nodes[i].transitions;
	for(int j = 0; j < transitions.size(); j++) {
	    cout << " " << transitions[j].target->value << endl;
	    vector<ValueTransitionLabel> labels = transitions[j].labels;
	    for(int k = 0; k < labels.size(); k++) {
		cout << "  at-start-durations:" << endl;
		cout << "   " << labels[k].duration_variable << endl;
		cout << "  prevails:" << endl;
		vector<PrevailCondition> prevail = labels[k].all_prevails;
		for(int l = 0; l < prevail.size(); l++) {
		    cout << "   " << prevail[l].local_var << ":"
			    << prevail[l].value << endl;
		}
	    }
	}
	cout << " global parents:" << endl;
	for(int i = 0; i < ccg_parents.size(); i++)
	    cout << ccg_parents[i] << endl;
    }
}

void DomainTransitionGraphSymb::get_successors(int value, vector<int> &result) const {
    assert(result.empty());
    assert(value >= 0 && value < nodes.size());
    const vector<ValueTransition> &transitions = nodes[value].transitions;
    result.reserve(transitions.size());
    for(int i = 0; i < transitions.size(); i++)
	result.push_back(transitions[i].target->value);
}

DomainTransitionGraphSubterm::DomainTransitionGraphSubterm(int var_index) {
    is_axiom = g_axiom_layers[var_index] != -1;
    var = var_index;
}

void DomainTransitionGraphSubterm::read_data(istream &in) {
    check_magic(in, "begin_DTG");
    in >> left_var >> op >> right_var;
    check_magic(in, "end_DTG");
}

void DomainTransitionGraphSubterm::dump() const {
    cout << " (subterm)" << endl;
    cout << left_var << " " << op << " " << right_var << endl;
}

DomainTransitionGraphFunc::DomainTransitionGraphFunc(int var_index) {
    is_axiom = g_axiom_layers[var_index] != -1;
    var = var_index;
}

void DomainTransitionGraphFunc::read_data(istream &in) {
    check_magic(in, "begin_DTG");
    int number_of_transitions;
    in >> number_of_transitions;
    for(int i = 0; i < number_of_transitions; i++) {
	int op_index;
	in >> op_index;
	Operator *op = &g_operators[op_index];
	assignment_op a_op;
	int influencing_variable;
	in >> a_op >> influencing_variable;
	int duration_variable;
	vector<PrevailCondition> prevails;

	binary_op bop;
	in >> bop >> duration_variable;

	if(bop != eq) {
	    cout << "Error: The duration constraint must be of the form\n";
	    cout << "       (= ?duration (arithmetic_term))" << endl;
	    exit(1);
	}

	int count;

	in >> count; //number of  Conditions
	for(int i = 0; i < count; i++) {
	    PrevailCondition prev_cond = PrevailCondition(in);
	    prev_cond.prev_dtg = g_transition_graphs[prev_cond.local_var];
	    prevails.push_back(prev_cond);
	}
	transitions.push_back(FuncTransition(var, prevails, a_op,
		influencing_variable, duration_variable, op));
    }

    check_magic(in, "end_DTG");

}

PrevailCondition::PrevailCondition(istream &in) {
    in >> local_var >> value;
}

void PrevailCondition::dump() const {
    cout << local_var << ": " << value;
}

void DomainTransitionGraphFunc::dump() const {
    cout << " (functional)" << endl;
    for(int i = 0; i < transitions.size(); i++) {
	cout << " durations_at_start:" << endl;
	cout << "  " << transitions[i].duration_variable << endl;
	cout << " conds:" << endl;
	for(int j = 0; j < transitions[i].all_prevails.size(); j++) {
	    cout << "  ";
	    transitions[i].all_prevails[j].dump();
	}
	cout << endl;
	cout << " effect: ";
	cout << transitions[i].a_op << " "
		<< transitions[i].influencing_variable << endl;
    }
}

DomainTransitionGraphComp::DomainTransitionGraphComp(int var_index) {
    is_axiom = g_axiom_layers[var_index] != -1;
    var = var_index;
}

void DomainTransitionGraphComp::read_data(istream &in) {
    check_magic(in, "begin_DTG");
    check_magic(in, "1");
    in >> nodes.second.left_var >> nodes.second.op >> nodes.second.right_var;
    check_magic(in, "0");
    in >> nodes.first.left_var >> nodes.first.op >> nodes.first.right_var;
    check_magic(in, "end_DTG");
}

void DomainTransitionGraphComp::compute_recursively_parents(int var,
	map<int, int> &global_to_ccg_parent) {
    if(g_variable_types[var] == primitive_functional) {
	// Processing for full DTG (cyclic CG).
	translate_global_to_local(global_to_ccg_parent, var);
	return;
    }
    if(g_variable_types[var] == subterm_functional) {
	translate_global_to_local(global_to_ccg_parent, var);
	DomainTransitionGraph* dtg = g_transition_graphs[var];
	DomainTransitionGraphSubterm* sdtg =
		dynamic_cast<DomainTransitionGraphSubterm*>(dtg);
	assert(sdtg);
	compute_recursively_parents(sdtg->get_left_var(), global_to_ccg_parent);
	compute_recursively_parents(sdtg->get_right_var(), global_to_ccg_parent);
	return;
    }
    assert(false);
}

int DomainTransitionGraphComp::translate_global_to_local(
	map<int, int> &global_to_ccg_parent, int global_var) {
    int local_var;
    if(!global_to_ccg_parent.count(global_var)) {
	local_var = ccg_parents.size();
	global_to_ccg_parent[global_var] = local_var;
//	cout << "global_var: " << global_var << ", local_var: " << local_var << endl;
	ccg_parents.push_back(global_var);
	global_to_local_ccg_parents.insert(ValuePair(global_var, local_var));
    } else {
	local_var = global_to_ccg_parent[global_var];
    }
    return local_var;
}

void DomainTransitionGraphComp::collect_recursively_func_transitions(int var,
	map<int, int> &global_to_ccg_parent) {
    DomainTransitionGraph* dtg = g_transition_graphs[var];
    if(g_variable_types[var] == primitive_functional) {
	DomainTransitionGraphFunc* fdtg =
		dynamic_cast<DomainTransitionGraphFunc*>(dtg);
	assert(fdtg);
	for(int i = 0; i < fdtg->transitions.size(); i++) {
	    transitions.push_back(fdtg->transitions[i]);
	    FuncTransition &trans = transitions.back();
	    int starting_variable_global =
		trans.starting_variable;
	    int starting_variable_local =
		    translate_global_to_local(global_to_ccg_parent, starting_variable_global);
	    trans.starting_variable = starting_variable_local;

	    int influencing_variable_global =
		    trans.influencing_variable;
	    int influencing_variable_local =
		    translate_global_to_local(global_to_ccg_parent, influencing_variable_global);
	    trans.influencing_variable
		    = influencing_variable_local;

	    if(trans.duration_variable != -1) {
		int duration_variable_global =
			trans.duration_variable;
		int duration_variable_local =
			translate_global_to_local(global_to_ccg_parent, duration_variable_global);
		trans.duration_variable = duration_variable_local;
	    }
	    vector<PrevailCondition> *prevails =
		    &(trans.all_prevails);
	    for(int k = 0; k < prevails->size(); k++) {
		int global_var = (*prevails)[k].local_var;
		int local_var = translate_global_to_local(global_to_ccg_parent, global_var);
		(*prevails)[k].local_var = local_var;
	    }
	}
	return;
    }
    if(g_variable_types[var] == subterm_functional) {
	DomainTransitionGraphSubterm* sdtg =
		dynamic_cast<DomainTransitionGraphSubterm*>(dtg);
	assert(sdtg);
	collect_recursively_func_transitions(sdtg->get_left_var(),
		global_to_ccg_parent);
	collect_recursively_func_transitions(sdtg->get_right_var(),
		global_to_ccg_parent);
	return;
    }
}

void DomainTransitionGraphComp::dump() const {
    cout << " (comparison)" << endl;
    cout << "1: " << nodes.first.left_var << " " << nodes.first.op << " "
	    << nodes.first.right_var << endl;
    cout << "0: " << nodes.second.left_var << " " << nodes.second.op << " "
	    << nodes.second.right_var << endl;
    cout << "transitions: " << endl;
    for(int i = 0; i < transitions.size(); i++) {
	cout << " duration variable: ";
	cout << "  " << transitions[i].duration_variable;
	cout << endl;
	cout << " conds:" << endl;
	for(int j = 0; j < transitions[i].all_prevails.size(); j++) {
	    cout << "  ";
	    transitions[i].all_prevails[j].dump();
	}
	cout << endl;
	cout << " effect: ";
	cout << transitions[i].a_op << " "
		<< transitions[i].influencing_variable << endl;
    }
    cout << "Context: ";
    for(int i = 0; i< ccg_parents.size(); i++) {
	cout << ccg_parents[i] << " ";
    }
    cout << endl;
}

class hash_pair_vector {
public:
    size_t operator()(const vector<pair<int, int> > &vec) const {
	unsigned long hash_value = 0;
	for(int i = 0; i < vec.size(); i++) {
	    hash_value = 17 * hash_value + vec[i].first;
	    hash_value = 17 * hash_value + vec[i].second;
	}
	return size_t(hash_value);
    }
};

void ValueTransition::simplify() {
    // Remove labels with duplicate or dominated conditions.

    /*
     Algorithm: Put all transitions into a hash_map
     (key: condition, value: index in label list).
     This already gets rid of transitions with identical conditions.

     Then go through the hash_map, checking for each element if
     none of the subset conditions are part of the hash_map.
     Put the element into the new labels list iff this is the case.
     */

    typedef vector<pair<int, int> > HashKey;
    typedef hash_map<HashKey, int, hash_pair_vector> HashMap;
    HashMap label_index;
    label_index.resize(labels.size() * 2);

    for(int i = 0; i < labels.size(); i++) {
	HashKey key;
	const vector<PrevailCondition> &conditions = labels[i].all_prevails;
	for(int j = 0; j < conditions.size(); j++)
	    key.push_back(make_pair(conditions[j].local_var,
		    static_cast<int>(conditions[j].value)));
	sort(key.begin(), key.end());
	label_index[key] = i;
    }

    vector<ValueTransitionLabel> old_labels;
    old_labels.swap(labels);

    for(HashMap::iterator it = label_index.begin(); it != label_index.end(); ++it) {
	const HashKey &key = it->first;
	int label_no = it->second;
	int powerset_size = (1 << key.size()) - 1; // -1: only consider proper subsets
	bool match = false;
	if(powerset_size <= 31) { // HACK! Don't spend too much time here...
	    for(int mask = 0; mask < powerset_size; mask++) {
		HashKey subset;
		for(int i = 0; i < key.size(); i++)
		    if(mask & (1 << i))
			subset.push_back(key[i]);
		if(label_index.count(subset)) {
		    match = true;
		    break;
		}
	    }
	}
	if(!match)
	    labels.push_back(old_labels[label_no]);
    }
}

