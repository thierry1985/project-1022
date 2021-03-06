#include "heuristic.h"
#include "operator.h"

using namespace std;

Heuristic::Heuristic(bool use_caching) {
    use_cache = use_caching;
    heuristic = NOT_INITIALIZED;
}

Heuristic::~Heuristic() {
}

void Heuristic::set_preferred(const Operator *op) {
    for(int i = 0; i < preferred_operators.size(); i++) {
	if(preferred_operators[i] == op) {
	    return;
	}
    }
    preferred_operators.push_back(op);
}

void Heuristic::evaluate(const TimeStampedState &state) {
    if(use_cache) {
	map<TimeStampedState, EvaluationInfo>::iterator it =
		state_cache.find(state);
	if(it != state_cache.end()) {
	    heuristic = it->second.heuristic;
	    preferred_operators = it->second.preferred_operators;
	    return;
	}
    }

    if(heuristic == NOT_INITIALIZED)
	initialize();
    preferred_operators.clear();
    heuristic = compute_heuristic(state);
    assert(heuristic == DEAD_END || heuristic >= 0);

    if(heuristic == DEAD_END) {
	// It is ok to have preferred operators in dead-end states.
	// This allows a heuristic to mark preferred operators on-the-fly,
	// selecting the first ones before it is clear that all goals
	// can be reached.
	preferred_operators.clear();
    }

    if(use_cache) {
	EvaluationInfo info(heuristic, preferred_operators);
	state_cache[state] = info;
    }

#ifndef NDEBUG
    if(heuristic != DEAD_END) {
//	cout << "preferred operators:" << endl;
//	for(int i = 0; i < preferred_operators.size(); i++) {
//	    //assert(preferred_operators[i]->is_applicable(state));
//	    cout << "  " << preferred_operators[i]->get_name() << endl;
//	}
    }
#endif
}

bool Heuristic::is_dead_end() {
    return heuristic == DEAD_END;
}

double Heuristic::get_heuristic() {
    // The -1 value for dead ends is an implementation detail which is
    // not supposed to leak. Thus, calling this for dead ends is an
    // error. Call "is_dead_end()" first.
    assert(heuristic >= 0);
    return heuristic;
}

void Heuristic::get_preferred_operators(std::vector<const Operator *> &result) {
    assert(heuristic >= 0);
    result.insert(result.end(), preferred_operators.begin(),
	    preferred_operators.end());
}
