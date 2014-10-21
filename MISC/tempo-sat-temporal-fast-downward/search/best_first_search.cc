#include "best_first_search.h"

#include "globals.h"
#include "heuristic.h"
#include "successor_generator.h"
#include "transition_cache.h"

#include <cassert>
using namespace std;

OpenListInfo::OpenListInfo(Heuristic *heur, bool only_pref) {
    heuristic = heur;
    only_preferred_operators = only_pref;
    priority = 0;
}

BestFirstSearchEngine::BestFirstSearchEngine() :
    current_state(*g_initial_state) {
    generated_states = 0;
    current_predecessor = 0;
    current_operator = 0;
    last_time = time(NULL);
}

BestFirstSearchEngine::~BestFirstSearchEngine() {
}

void BestFirstSearchEngine::add_heuristic(Heuristic *heuristic,
	bool use_estimates, bool use_preferred_operators) {

    // cout << "Adding heuristic" << endl;

    assert(use_estimates || use_preferred_operators);
    heuristics.push_back(heuristic);
    best_heuristic_values.push_back(-1);
    if(use_estimates) {
	open_lists.push_back(OpenListInfo(heuristic, false));
	open_lists.push_back(OpenListInfo(heuristic, true));
    }
    if(use_preferred_operators)
	preferred_operator_heuristics.push_back(heuristic);
}

void BestFirstSearchEngine::initialize() {
    cout << "Conducting best first search." << endl;
    assert(!open_lists.empty());
}

void BestFirstSearchEngine::statistics() const {
    cout << "Expanded " << closed_list.size() << " state(s)." << endl;
    cout << "Generated " << generated_states << " state(s)." << endl;
}

void BestFirstSearchEngine::dump_transition() const {
    cout << endl;
    if(current_predecessor != 0) {
	cout << "DEBUG: In step(), current predecessor is: " << endl;
	current_predecessor->dump();
    }
    cout << "DEBUG: In step(), current operator is: ";
    if(current_operator != 0) {
	current_operator->dump();
    } else {
	cout << "No operator before initial state." << endl;
    }
    cout << "DEBUG: In step(), current state is: " << endl;
    current_state.dump();
    cout << endl;
}

int BestFirstSearchEngine::step() {
    
    // Invariants:
    // - current_state is the next state for which we want to compute the heuristic.
    // - current_predecessor is a permanent pointer to the predecessor of that state.
    // - current_operator is the operator which leads to current_state from predecessor.

    

//    dump_transition();
    bool discard = true;
    if (closed_list.contains(current_state)) {
	double diff = (current_state.timestamp - closed_list.get(current_state).timestamp);
	if(diff < 0) discard = false;
    }
    else discard = false;

    if(!discard) {

	//	cout << "DEBUG: inserting into closed list the tuple (s',s,op): " << endl;
	//	cout << "current state:" << endl;
	//	current_state.dump();
	//	cout << "predecessor:" << endl;
	//	if(current_predecessor) current_predecessor->dump(); else cout << "NULL" << endl;
	//	cout << "current operator:" << endl;
	//	if(current_operator) current_operator->dump(); else cout << "NULL" << endl;
	//	cout << endl;

	const TimeStampedState *parent_ptr = closed_list.insert(current_state,
		current_predecessor, current_operator);
	for(int i = 0; i < heuristics.size(); i++)
	    heuristics[i]->evaluate(current_state);
	if(!is_dead_end()) {
	    if(check_goal())
		//		return IN_PROGRESS;
		return SOLVED;
	    if(check_progress()) {
//		cout << "current operator:" << endl;
//		if(current_operator)
//		    current_operator->dump();
//		else
//		    cout << "NULL" << endl;
		report_progress();
		reward_progress();
	    }
	    generate_successors(parent_ptr);
	}
//	cout << "bla" << endl;
    } else {
//	cout << "blubb" << endl;
//	double diff = (current_state.timestamp - closed_list.get(current_state).timestamp);
//	if(diff != 0) cout << "Difference of time stamps = " << diff << endl;
	//	cout << "Debug: closed_list.contains(current_state)" << endl;
//	cout << "Debug: closed_list.size() = " << closed_list.size() << endl;
    }
    
//    time_t seconds;
//    seconds = time(NULL);
//    if(seconds % 10 == 0 && seconds > last_time) {
//	statistics();
//	last_time = seconds;
//	dump_plan_prefix_for_current_state();
//	cout << endl;
//    }
    
    return fetch_next_state();
}

bool BestFirstSearchEngine::is_dead_end() {
    // If a reliable heuristic reports a dead end, we trust it.
    // Otherwise, all heuristics must agree on dead-end-ness.
    int dead_end_counter = 0;
    for(int i = 0; i < heuristics.size(); i++) {
	if(heuristics[i]->is_dead_end()) {
	    
//	    cout << "BestFirstSearchEngine::is_dead_end(): heuristic has reported dead end." << endl;
	    
	    if(heuristics[i]->dead_ends_are_reliable())
		return true;
	    else
		dead_end_counter++;
	}
    }
    return dead_end_counter == heuristics.size();
}

bool BestFirstSearchEngine::check_goal() {
    
//    cout << "BestFirstSearchEngine::check_goal()" << endl;
    
//    if(!current_state.operators.empty()) return false;
//    if(!current_state.satisfies(g_goal)) return false;
    
    // Any heuristic reports 0 iff this is a goal state, so we can
    // pick an arbitrary one.
    Heuristic *heur = open_lists[0].heuristic;
    if(!heur->is_dead_end() && heur->get_heuristic() == 0) {
	
	
	    if(!current_state.operators.empty() ||
		    !current_state.satisfies(g_goal)) {
		cout << "Heuristic value is ZERO, but we have not yet reached a goal state" << endl;
		cout << "Still active ops left?" << !current_state.operators.empty() << endl;
		cout << "Goal not satisfied?" << !current_state.satisfies(g_goal) << endl;
		return false;
	    }
	
	// We actually need this silly !heur->is_dead_end() check because
	// this state *might* be considered a non-dead end by the
	// overall search even though heur considers it a dead end
	// (e.g. if heur is the CG heuristic, but the FF heuristic is
	// also computed and doesn't consider this state a dead end.
	// If heur considers the state a dead end, it cannot be a goal
	// state (heur will not be *that* stupid). We may not call
	// get_heuristic() in such cases because it will barf.
	cout << "Solution found!" << endl;
	Plan plan;
	closed_list.trace_path(current_state, plan);
	set_plan(plan);
	return true;
    } else {
	return false;
    }
}

void BestFirstSearchEngine::dump_plan_prefix_for_current_state() const {
    Plan plan;
    closed_list.trace_path(current_state, plan);
//    cout << "Plan prefix length is " << plan.size() << endl;
    for(int i = 0; i < plan.size(); i++) {
	const PlanStep& step = plan[i];
	cout << step.start_time << ": "<< "(" << step.op->get_name() << ")" << " ["  << step.duration << "]"<< endl;
    }
    cout << "Plan length: " << plan.size() << " step(s)." << endl;
}

bool BestFirstSearchEngine::check_progress() {
    bool progress = false;
    for(int i = 0; i < heuristics.size(); i++) {
	if(heuristics[i]->is_dead_end())
	    continue;
	double h = heuristics[i]->get_heuristic();
	double &best_h = best_heuristic_values[i];
	if(best_h == -1 || h < best_h) {
	    best_h = h;
	    progress = true;
	}
    }
    return progress;
}

void BestFirstSearchEngine::report_progress() {
    cout << "Best heuristic value: ";
    for(int i = 0; i < heuristics.size(); i++) {
	cout << best_heuristic_values[i];
	if(i != heuristics.size() - 1)
	    cout << "/";
    }
    cout << " [expanded " << closed_list.size() << " state(s)]" << endl;
}

void BestFirstSearchEngine::reward_progress() {
    // Boost the "preferred operator" open lists somewhat whenever
    // progress is made. This used to be used in multi-heuristic mode
    // only, but it is also useful in single-heuristic mode, at least
    // in Schedule.
    //
    // TODO: Test the impact of this, and find a better way of rewarding
    // successful exploration. For example, reward only the open queue
    // from which the good state was extracted and/or the open queues
    // for the heuristic for which a new best value was found.

    for(int i = 0; i < open_lists.size(); i++)
	if(open_lists[i].only_preferred_operators)
	    open_lists[i].priority -= 1000;
}

void BestFirstSearchEngine::generate_successors(
	const TimeStampedState *parent_ptr) {
    vector<const Operator *> all_operators;
    g_successor_generator->generate_applicable_ops(current_state, all_operators);

    vector<const Operator *> preferred_operators;
    for(int i = 0; i < preferred_operator_heuristics.size(); i++) {
	Heuristic *heur = preferred_operator_heuristics[i];
	if(!heur->is_dead_end())
	    heur->get_preferred_operators(preferred_operators);
    }

//    cout << "Preferred operators: " << endl;
//    for(int i = 0; i < preferred_operators.size(); i++) {
//	preferred_operators[i]->dump();
//    }
    
    //    cout << "DEBUG: current_state is " << endl;
    //    current_state.dump();
    //    cout << "DEBUG: all_operators.size() = " << all_operators.size() << endl;
    //    cout << "DEBUG: open_lists.size() = " << open_lists.size() << endl;

    for(int i = 0; i < open_lists.size(); i++) {

	//	cout << "Debug: For open_lists[" << i << "]: " << endl;

	Heuristic *heur = open_lists[i].heuristic;
	if(!heur->is_dead_end()) {
	    double h = heur->get_heuristic();
	    double val = 0;
	    if(g_greedy) {
	        val = h;
	    } else {
	        val = h + parent_ptr->timestamp;
	    }
	    OpenList<OpenListEntry> &open = open_lists[i].open;
	    vector<const Operator *>
		    &ops =
			    open_lists[i].only_preferred_operators ? preferred_operators
				    : all_operators;

	    //cout << "Debug: Truly applicable operators in current state:" << endl;

	    for(int j = 0; j < ops.size(); j++) {
		if(ops[j]->is_applicable(*parent_ptr)) {
		    //TODO: In order to use the bucket implementation of an open list, we have to cast
		    //the heuristic value, which is a double value, to an int. Check whether this is passable.
		    open.insert(static_cast<int>(val), make_pair(parent_ptr,
			    ops[j]));
		    generated_states++;

		    //		    cout << "       Operator no. " << j << ": ";
		    //		    ops[j]->dump();
		}
	    }

	    //	    cout << "DEBUG: *parent_ptr = " << endl;
	    //	    parent_ptr->dump();
	    //	    cout << "DEBUG: parent_ptr->next_happening() = " << parent_ptr->next_happening() << endl;
	    //	    cout << "DEBUG: parent_ptr->timestamp        = " << parent_ptr->timestamp << endl;

	    //	    if(parent_ptr->next_happening() != parent_ptr->timestamp) {
	    // there exists another happening in the future, so
	    // we can let time pass until then, which is represented
	    // here as a NULL pointer

	    //		cout << "Debug: Inserting dummy operator in open list." << endl;

	    // Only add a no-op if letting time pass does not
	    // lead to an inconsistent state. This test is necessary for
	    // the following reason: Assume that the current state does
	    // not yet satisfy the over-all condition of an operator
	    // scheduled to start in the current state. Then we have
	    // to schedule another operator for application right now
	    // which has an at-start effect asserting this over-all
	    // condition. Therefore, although the current state CAN
	    // be a prefix of a valid plan, the NO-OP operator is
	    // not applicable in the current state. This issue can be
	    // checked by calling parent_ptr->is_consistent_when_progressed().
	    //		cout << "DEBUG: Calling is_consistent_when_progressed() from generate_successors()." << endl;
	    if(parent_ptr->is_consistent_when_progressed()) {
		open.insert(static_cast<int>(val), make_pair(parent_ptr,
			g_let_time_pass));
		generated_states++;
	    } else {
		//		    cout << "DEBUG: parent_ptr->is_consistent_when_progressed() is FALSE for *parent_ptr = " << endl;
		//		    parent_ptr->dump();
	    }
	    //	    } else {
	    //		cout << "DEBUG: parent_ptr->next_happening() == parent_ptr->timestamp for *parent_ptr = " << endl;
	    //		parent_ptr->dump();		
	    //	    }

	}
    }
    // generated_states += all_operators.size();
}

int BestFirstSearchEngine::fetch_next_state() {
    OpenListInfo *open_info = select_open_queue();
    if(!open_info) {
	cout << "Completely explored state space -- no solution!" << endl;
	return FAILED;
    }

    pair<const TimeStampedState *, const Operator *> next_pair =
	    open_info->open.remove_min();
    open_info->priority++;

    current_predecessor = next_pair.first;
    current_operator = next_pair.second;

    //    cout << "DEBUG: current_predecessor:" << endl;
    //    current_predecessor->dump();
    //    
    //    cout << "DEBUG: current_operator:" << endl;
    //    current_operator->dump();

    //    if(g_transition_cache->is_cached(*current_predecessor, *current_operator)) {
    //	current_state =
    //	    g_transition_cache->lookup(*current_predecessor, *current_operator);
    //
    ////	cout << "DEBUG: found cached transition" << endl;
    //
    //    } else {
    if(current_operator == g_let_time_pass) {
	// do not apply an operator but rather let some time pass until
	// next scheduled happening
	current_state = current_predecessor->let_time_pass();
    } else {
	current_state = TimeStampedState(*current_predecessor,
		*current_operator);
    }
    //	g_transition_cache->store(*current_predecessor, *current_operator, current_state);
    //    }
    return IN_PROGRESS;
}

OpenListInfo *BestFirstSearchEngine::select_open_queue() {
    OpenListInfo *best = 0;
    for(int i = 0; i < open_lists.size(); i++) {
	//	cout << "DEBUG: In select_open_queue: open_lists[" << i <<
	//	"].open.empty()? " << (open_lists[i].open.empty() ? "yes" : "no") << endl;
	if(!open_lists[i].open.empty() && (best == 0 || open_lists[i].priority < best->priority))
	    best = &open_lists[i];
    }

    // cout << "DEBUG: selected open queue is empty? " << (best->open.empty() ? "yes" : "no") << endl;

    return best;
}
