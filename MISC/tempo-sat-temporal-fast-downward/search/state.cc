#include "state.h"

#include "axioms.h"
#include "globals.h"
#include "operator.h"
#include "causal_graph.h"
// #include "transition_cache.h"

#include <algorithm>
#include <iostream>
#include <cassert>
using namespace std;

TimeStampedState::TimeStampedState(istream &in) {
    check_magic(in, "begin_state");
    for(int i = 0; i < g_variable_domain.size(); i++) {
	double var;
	cin >> var;
	state.push_back(var);
	dirty_bits.push_back(false);
    }
    check_magic(in, "end_state");

    g_default_axiom_values = state;
    timestamp = 0.0 + EPS_TIME;
    
    initialize();
}

TimeStampedState::TimeStampedState(const TimeStampedState &predecessor,
	const Operator &op) :
	    state(predecessor.state),
	    dirty_bits(predecessor.dirty_bits),
	    scheduled_effects(predecessor.scheduled_effects),
	    conds_over_all(predecessor.conds_over_all),
	    conds_at_end(predecessor.conds_at_end),
	    operators(predecessor.operators) {
    
    timestamp = predecessor.timestamp;
    
    // compute duration
    
//    cout << "DEBUG: op.get_duration_var() = " << op.get_duration_var() << endl;
//    cout << "DEBUG: predecessor[op.get_duration_var()] = " << predecessor[op.get_duration_var()] << endl;
    
    double duration = predecessor[op.get_duration_var()];
//    double t_end = timestamp + duration;

    // Update values affected by an at-start effect of the operator.
    for(int i = 0; i < op.get_pre_post_start().size(); i++) {
	const PrePost &pre_post = op.get_pre_post_start()[i];

	// at-start effects may not have any at-end conditions
	assert(pre_post.cond_end.size() == 0);

	if(pre_post.does_fire(predecessor)) {
	    apply_effect(pre_post.var, pre_post.fop, pre_post.var_post,
		    pre_post.post);
	    // dirty_bits[pre_post.var] = true;
	}
    }

    g_axiom_evaluator->evaluate(*this);

    // The scheduled effects of the new state are precisely the
    // scheduled effects of the predecessor state plus those at-end
    // effects of the given operator whose at-start conditions are
    // satisfied.
    for(int i = 0; i < op.get_pre_post_end().size(); i++) {
	const PrePost &eff = op.get_pre_post_end()[i];
	if(eff.does_fire(predecessor)) {
	    scheduled_effects.push_back(ScheduledEffect(duration, eff));
	}
    }

    // The persistent over-all conditions of the new state are
    // precisely the persistent over-all conditions of the predecessor
    // state plus the over-all conditions of the newly added operator
    for(int i = 0; i < op.get_prevail_overall().size(); i++) {
	conds_over_all.push_back(ScheduledCondition(duration,
		op.get_prevail_overall()[i]));
    }

    // The persistent at-end conditions of the new state are
    // precisely the persistent at-end conditions of the predecessor
    // state plus the at-end conditions of the newly added operator
    for(int i = 0; i < op.get_prevail_end().size(); i++) {
	conds_at_end.push_back(ScheduledCondition(duration, op.get_prevail_end()[i]));
    }
    
    // The running operators of the new state are precisely
    // the running operators of the predecessor state plus the newly
    // added operator
    operators.push_back(ScheduledOperator(duration,op));
    
    
        timestamp += EPS_TIME;
        assert(!double_equals(timestamp, next_happening()));
    
        for(int i = 0; i < dirty_bits.size(); i++)
            dirty_bits[i] = false; 
        
    initialize();
}

TimeStampedState TimeStampedState::let_time_pass(
	bool go_to_intermediate_between_now_and_next_happening) const {
    // Copy this state
    TimeStampedState succ(*this);
    
    // The time stamp of the new state is the minimum of the end
    // time points of scheduled effects in the predecessor state
    // and the end time points associated with persistent at-end
    // conditions of the predecessor state. If the flag "go to
    // intermediate between now and next happening" is set, it is
    // the intermediate time point between new and the next happening
    // (this is needed to safely test all persistent over-all
    // conditions -- otherwise we might fail to ever test some of them).
    double nh = next_happening();
    if(double_equals(nh,timestamp) ||
	    !go_to_intermediate_between_now_and_next_happening) {
	succ.timestamp = nh;
    } else {
	succ.timestamp = timestamp + 0.5*(nh - timestamp);
    }
        
    // Handle the borderline case where there are no more happenings
    // scheduled, but an operator application is currently being
    // finished, possibly preventing an application of another operator
    // right now, but admitting it after an infinitesimal amount of time
    // has passed. Then let some time pass.
    if(double_equals(succ.timestamp,timestamp)) {
	bool dirty = false;
	for(int i = 0; i < dirty_bits.size(); i++) {
	    dirty |= dirty_bits[i];
	}
	if(dirty) succ.timestamp += EPS_TIME;
    }

    // Reset dirty bits for modified state variables
    if(succ.timestamp > timestamp) {
	for(int i = 0; i < succ.dirty_bits.size(); i++)
	    succ.dirty_bits[i] = false;
    }
    
    double time_diff = succ.timestamp - timestamp;
    
    // The values of the new state are obtained by applying all
    // effects scheduled in the predecessor state for the new time
    // stamp and subsequently applying axioms
    for(int i = 0; i < scheduled_effects.size(); i++) {
	const ScheduledEffect &eff = scheduled_effects[i];
	if(double_equals(eff.time_increment,time_diff) &&
		succ.satisfies(eff.cond_end)) {
	    succ.apply_effect(eff.var,eff.fop,eff.var_post,eff.post);
	}
    }
    g_axiom_evaluator->evaluate(succ);

    // The scheduled effects of the new state are precisely the
    // scheduled effects of the predecessor state minus those
    // whose scheduled time point has been reached and minus those
    // whose over-all condition is violated.
    for(int i = 0; i < succ.scheduled_effects.size(); i++) {
	succ.scheduled_effects[i].time_increment -= time_diff;
    }
    for(int i = 0; i < succ.scheduled_effects.size(); i++) {
	const ScheduledEffect &eff = succ.scheduled_effects[i];
	if(double_equals(eff.time_increment,0) ||
		!succ.satisfies(eff.cond_overall,true)) {
	    succ.scheduled_effects.erase(succ.scheduled_effects.begin()+i);
	    i--;
	}
    }

    // The persistent over-all conditions of the new state are
    // precisely those persistent over-all conditions of the predecessor
    // state whose end time-point is properly in the future (not now)
    for(int i = 0; i < succ.conds_over_all.size(); i++) {
	succ.conds_over_all[i].time_increment -= time_diff;
    }
    for(int i = 0; i < succ.conds_over_all.size(); i++) {
	const ScheduledCondition &cond = succ.conds_over_all[i];
	if(cond.time_increment <= 0) {
	    succ.conds_over_all.erase(succ.conds_over_all.begin()+i);
	    i--;
	}
    }
    
    // The persistent at-end conditions of the new state are
    // precisely those persistent at-end conditions of the predecessor
    // state whose end time-point is now or in the future
    for(int i = 0; i < succ.conds_at_end.size(); i++) {
	succ.conds_at_end[i].time_increment -= time_diff;
    }
    for(int i = 0; i < succ.conds_at_end.size(); i++) {
	const ScheduledCondition &cond = succ.conds_at_end[i];
	if(cond.time_increment < 0) {
	    succ.conds_at_end.erase(succ.conds_at_end.begin()+i);
	    i--;
	}
    }
    
    // The running operators of the new state are precisely those
    // running operators of the predecessor state whose end time-point
    // is now or in the future
    for(int i = 0; i < succ.operators.size(); i++) {
	succ.operators[i].time_increment -= time_diff;
    }
    for(int i = 0; i < succ.operators.size(); i++) {
	const ScheduledOperator &op = succ.operators[i];
	if(op.time_increment < 0) {
	    succ.operators.erase(succ.operators.begin()+i);
	    i--;
	}
    }
    
    succ.initialize();
    
    return succ;
}












double TimeStampedState::next_happening() const {
    double result = REALLYBIG;
    // double result = timestamp;
    for(int i = 0; i < scheduled_effects.size(); i++)
	result = min(result, scheduled_effects[i].time_increment);
    for(int i = 0; i < conds_over_all.size(); i++)
    	result = min(result, conds_over_all[i].time_increment);
//    for(int i = 0; i < conds_at_end.size(); i++)
//	result = min(result, conds_at_end[i].timepoint);
    for(int i = 0; i < operators.size(); i++)
	if(operators[i].time_increment > 0)
	    result = min(result, operators[i].time_increment);
    if(result == REALLYBIG)
	result = 0;
    return result + timestamp;
}

void TimeStampedState::dump() const {
    cout << "State (" << timestamp << ")" << endl;
    cout << " state:" << endl;
    for(int i = 0; i < state.size(); i++)
	cout << "  " << g_variable_name[i] << ": " << state[i] << "    "  << dirty_bits[i] << endl;
    cout << " scheduled effects:" << endl;
    for(int i = 0; i < scheduled_effects.size(); i++) {
	cout << "  <" << (scheduled_effects[i].time_increment + timestamp) << ",<";
	for(int j = 0; j < scheduled_effects[i].cond_overall.size(); j++) {
	    cout << g_variable_name[scheduled_effects[i].cond_overall[j].var]
		    << ": " << scheduled_effects[i].cond_overall[j].prev;
	}
	cout <<">,<";
	for(int j = 0; j < scheduled_effects[i].cond_end.size(); i++) {
	    cout << g_variable_name[scheduled_effects[i].cond_end[j].var]
		    << ": " << scheduled_effects[i].cond_end[j].prev;
	}
	cout <<">,<";
	cout << g_variable_name[scheduled_effects[i].var] << " ";
	if(is_functional(scheduled_effects[i].var)) {
	    cout << scheduled_effects[i].fop << " ";
	    cout << g_variable_name[scheduled_effects[i].var_post] << ">>" << endl;
	} else {
	    cout << ":= ";
	    cout << scheduled_effects[i].post << ">>" << endl;
	}
    }
    cout << " persistent over-all conditions:" << endl;
    for(int i = 0; i < conds_over_all.size(); i++) {
	cout << "  <" << (conds_over_all[i].time_increment + timestamp) << ",<";
	cout << g_variable_name[conds_over_all[i].var] << ":"
		<< conds_over_all[i].prev << ">>" << endl;;
	//cout << ">>" << endl;
    }
    cout << " persistent at-end conditions:" << endl;
    for(int i = 0; i < conds_at_end.size(); i++) {
	cout << "  <" << (conds_at_end[i].time_increment + timestamp) << ",<";
	cout << g_variable_name[conds_at_end[i].var] << ":"
		<< conds_at_end[i].prev << ">>" << endl;;
	//cout << ">>" << endl;
    }
    cout << " running operators:" << endl;
    for(int i = 0; i < operators.size(); i++) {
	cout << "  <" << (operators[i].time_increment + timestamp) << ",<";
	cout << operators[i].get_name() << ">>" << endl;;
    }
}

bool TimeStampedState::operator<(const TimeStampedState &other) const {
    if(timestamp < other.timestamp) return true;
    if(timestamp > other.timestamp) return false;
    if(lexicographical_compare(state.begin(), state.end(),
	    other.state.begin(), other.state.end())) return true;
    if(lexicographical_compare(other.state.begin(), other.state.end(),
	    state.begin(), state.end())) return false;
    if(lexicographical_compare(dirty_bits.begin(), dirty_bits.end(),
	    other.dirty_bits.begin(), other.dirty_bits.end())) return true;
    if(lexicographical_compare(other.dirty_bits.begin(), other.dirty_bits.end(),
	    dirty_bits.begin(), dirty_bits.end())) return false;
    if(scheduled_effects.size() < other.scheduled_effects.size()) return true;
    if(scheduled_effects.size() > other.scheduled_effects.size()) return false;
    if(conds_over_all.size() < other.conds_over_all.size()) return true;
    if(conds_over_all.size() > other.conds_over_all.size()) return false;
    if(conds_at_end.size() < other.conds_at_end.size()) return true;
    if(conds_at_end.size() > other.conds_at_end.size()) return false;
    if(lexicographical_compare(scheduled_effects.begin(), scheduled_effects.end(),
	    other.scheduled_effects.begin(), other.scheduled_effects.end())) return true;
    if(lexicographical_compare(other.scheduled_effects.begin(), other.scheduled_effects.end(),
	    scheduled_effects.begin(), scheduled_effects.end())) return false;
    if(lexicographical_compare(conds_over_all.begin(), conds_over_all.end(),
	    other.conds_over_all.begin(), other.conds_over_all.end())) return true;
    if(lexicographical_compare(other.conds_over_all.begin(), other.conds_over_all.end(),
	    conds_over_all.begin(), conds_over_all.end())) return false;
    if(lexicographical_compare(conds_at_end.begin(), conds_at_end.end(),
	    other.conds_at_end.begin(), other.conds_at_end.end())) return true;
    if(lexicographical_compare(other.conds_at_end.begin(), other.conds_at_end.end(),
	    conds_at_end.begin(), conds_at_end.end())) return false;
    return false;
}

void TimeStampedState::scheduleEffect(ScheduledEffect effect) {
    scheduled_effects.push_back(effect);
    sort(scheduled_effects.begin(), scheduled_effects.end());
}

bool TimeStampedState::is_consistent_now() const {
//    cout << "DEBUG: Call to is_consistent_now ..." << endl;
//    cout << "DEBUG: STATE:" << endl;
//    dump();
    
    // Persistent over-all conditions must be satisfied
    for(int i = 0; i < conds_over_all.size(); i++)
	if(!satisfies(conds_over_all[i],true))
	    return false;

    // Persistent at-end conditions must be satisfied
    // if their end time point is now
    for(int i = 0; i < conds_at_end.size(); i++)
	if(double_equals(conds_at_end[i].time_increment,0)
		&& !satisfies(conds_at_end[i]))
	    return false;
    
    // No further conditions (?)
    return true;
}

bool TimeStampedState::is_consistent_when_progressed() const {
    
    // First check for cached results    
//    if(g_consistency_cache->is_cached(*this)) {
//	return g_consistency_cache->lookup(*this);
//    }
//    vector<TimeStampedState> visited;
    
        
//    cout << "DEBUG: Call to is_consistent_when_progressed ..." << endl;
//    cout << "DEBUG: STATE:" << endl;
//    dump();
    
    double last_time = -1.0;
    double current_time = timestamp;
    TimeStampedState current_progression(*this);

    bool go_to_intermediate = true;
    while(!double_equals(current_time,last_time)) {
	
//	if(go_to_intermediate) visited.push_back(current_progression);
	
	if(!current_progression.is_consistent_now()) {
//	    cout << "DEBUG: This state is NOT consistent when progressed, since the" << endl;
//	    cout << "DEBUG: following intermediate state is inconsistent:" << endl;
//	    current_progression.dump();
	    
	    
	    // Cache result
//	    for(int i = 0; i < visited.size(); i++)
//		g_consistency_cache->store(visited[i],false);
	    return false;
	}

//	current_progression.dump();
	
	current_progression = current_progression.let_time_pass(
		go_to_intermediate);
	go_to_intermediate = !go_to_intermediate;
	last_time = current_time;
	current_time = current_progression.timestamp;
	
//	cout << "DEBUG: last_time    = " << last_time << endl;
//	cout << "DEBUG: current_time = " << current_time << endl;
	
    }
    
//    cout << "DEBUG: This state IS consistent when progressed" << endl;

    // Cache result
//    for(int i = 0; i < visited.size(); i++)
//	g_consistency_cache->store(visited[i],true);
    return true;
}

TimeStampedState &buildTestState(TimeStampedState &state) {
    vector<Prevail> cas;
    vector<Prevail> coa;
    vector<Prevail> cae;
    state.scheduleEffect(ScheduledEffect(1.0, cas, coa, cae, 11, 0, increase));
    state.scheduleEffect(ScheduledEffect(1.0, cas, coa, cae, 9, 0, assign));
    return state;
}
