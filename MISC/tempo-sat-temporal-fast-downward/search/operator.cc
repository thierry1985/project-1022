#include "globals.h"
#include "operator.h"

#include <iostream>
using namespace std;

Prevail::Prevail(istream &in) {
    in >> var >> prev;
}

bool Prevail::is_applicable(const TimeStampedState &state,
	const bool ignore_dirty_bits) const {
//    cout << "DEBUG: g_variabe_name.size(): " << g_variable_name.size() << endl;
//    cout << "DEBUG: var: " << var << endl;
    assert(var >= 0 && var < g_variable_name.size());
    assert(prev >= 0 && prev < g_variable_domain[var]);
    if(ignore_dirty_bits) {
	return double_equals(state[var],prev);
    }
    return double_equals(state[var],prev) && !state.dirty_bits[var];
}

PrePost::PrePost(istream &in) {
    int cond_count;
    in >> cond_count;
    for(int i = 0; i < cond_count; i++)
	cond_start.push_back(Prevail(in));
    in >> cond_count;
    for(int i = 0; i < cond_count; i++)
	cond_overall.push_back(Prevail(in));
    in >> cond_count;
    for(int i = 0; i < cond_count; i++)
	cond_end.push_back(Prevail(in));
    in >> var;
    if(is_functional(var)) {
	in >> fop >> var_post;
	// HACK: just use some arbitrary values for pre and post
	// s.t. they do not remain uninitialized
	pre = post = -1;
    } else {
	in >> pre >> post;
	// HACK: just use some arbitrary values for var_post and fop
	// s.t. they do not remain uninitialized
	var_post = -1;
	fop = assign;
    }
}

bool PrePost::is_applicable(const TimeStampedState &state) const {
    assert(var >= 0 && var < g_variable_name.size());
    assert(pre == -1 || pre >= 0 && pre < g_variable_domain[var]);
    
    for(int i = 0; i < cond_start.size(); i++) {
	if(state.dirty_bits[cond_start[i].var]) return false;
    }

    return pre == -1 || (double_equals(state[var],pre) && !state.dirty_bits[var]);
}

Operator::Operator(istream &in) {
    check_magic(in, "begin_operator");
    in >> ws;
    getline(in, name);
    int count;
    binary_op bop;
    in >> bop >> duration_var;
    if(bop != eq) {
	cout << "Error: The duration constraint must be of the form\n";
	cout << "       (= ?duration (arithmetic_term))" << endl;
	exit(1);
    }

    in >> count; //number of prevail at-start conditions
    for(int i = 0; i < count; i++)
	prevail_start.push_back(Prevail(in));
    in >> count; //number of prevail overall conditions
    for(int i = 0; i < count; i++)
	prevail_overall.push_back(Prevail(in));
    in >> count; //number of prevail at-end conditions
    for(int i = 0; i < count; i++)
	prevail_end.push_back(Prevail(in));
    in >> count; //number of pre_post_start conditions (symbolical)
    for(int i = 0; i < count; i++)
	pre_post_start.push_back(PrePost(in));
    in >> count; //number of pre_post_end conditions (symbolical)
    for(int i = 0; i < count; i++)
	pre_post_end.push_back(PrePost(in));
    in >> count; //number of pre_post_start conditions (functional)
    for(int i = 0; i < count; i++)
	pre_post_start.push_back(PrePost(in));
    in >> count; //number of pre_post_end conditions (functional)
    for(int i = 0; i < count; i++)
	pre_post_end.push_back(PrePost(in));
    check_magic(in, "end_operator");
}

Operator::Operator() {
    prevail_start   = vector<Prevail>();
    prevail_overall = vector<Prevail>();
    prevail_end     = vector<Prevail>();
    pre_post_start  = vector<PrePost>();
    pre_post_end    = vector<PrePost>();
    duration_var    = -1;
    name            = "let_time_pass";
}

void Prevail::dump() const {
    cout << g_variable_name[var] << ": " << prev;
}

void PrePost::dump() const {
    cout << g_variable_name[var] << ": " << pre << " => " << post;
    //    if(!cond.empty()) {
    //	cout << " if";
    //	for(int i = 0; i < cond.size(); i++) {
    //	    cout << " ";
    //	    cond[i].dump();
    //	}
    //    }
}

void Operator::dump() const {
    cout << name;
    // cout << ";";
    //    for(int i = 0; i < prevail.size(); i++) {
    //	cout << " [";
    //	prevail[i].dump();
    //	cout << "]";
    //    }
    //    for(int i = 0; i < pre_post.size(); i++) {
    //	cout << " [";
    //	pre_post[i].dump();
    //	cout << "]";
    //    }
    cout << endl;
}

bool Operator::is_applicable(const TimeStampedState &state) const {
    
//    cout << "DEBUG: Operator::is_applicable(...) called for operator " << get_name() << endl;

    
    if(state[duration_var] <= 0) {
//	cout << "DEBUG: The duration of the scheduled action would be " << state[duration_var] << endl;
//	cout << "DEBUG: The current state is" << endl;
//	state.dump();
//	cout << "DEBUG: The operator to be applied is" << endl;
//	dump();
	return false;
//	assert(state[duration_var] > 0);
    }
    
    for(int i = 0; i < prevail_start.size(); i++)
	if(!prevail_start[i].is_applicable(state))
	    return false;
    for(int i = 0; i < pre_post_start.size(); i++)
	if(!pre_post_start[i].is_applicable(state))
	    return false;
    
    // assert consistent at-start effects
    vector<bool> temp_dirty_bits(state.dirty_bits);
    for(int i = 0; i < pre_post_start.size(); i++) {
	if(pre_post_start[i].does_fire(state))
	    if(temp_dirty_bits[pre_post_start[i].var]) {
		return false;
	    } else {
		temp_dirty_bits[pre_post_start[i].var] = true;
	    }
    }
    
    // assert consistent at-end effects (conservatively)
    vector<bool> temp_dirty_bits_end;
    for(int i = 0; i < state.dirty_bits.size(); i++) {
	temp_dirty_bits_end.push_back(false);
    }    
    for(int i = 0; i < state.scheduled_effects.size(); i++) {	
	if(double_equals(state.scheduled_effects[i].time_increment,
	    state[duration_var])) {
	    temp_dirty_bits_end[state.scheduled_effects[i].var] = true;
	}
    }
    for(int i = 0; i < pre_post_end.size(); i++) {
	if(temp_dirty_bits_end[pre_post_end[i].var]) return false;
    }
    
    // There may be no simultaneous applications of two instances of the
    // same ground operator (for technical reasons, to simplify the task
    // of keeping track of durations committed to at the start of the
    // operator application)
    for(int i = 0; i < state.operators.size(); i++)
	if(state.operators[i].get_name() == get_name())
	    return false;

    return TimeStampedState(state,*this).is_consistent_when_progressed();
}

bool Operator::operator<(const Operator &other) const {
    return name < other.name;
}
