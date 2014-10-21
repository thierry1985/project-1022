#ifndef BEST_FIRST_SEARCH_H
#define BEST_FIRST_SEARCH_H

#include <vector>

#include "closed_list.h"
#include "open_list.h"
#include "search_engine.h"
#include "state.h"
#include "operator.h"

class Heuristic;
// class Operator;

typedef pair<const TimeStampedState *, const Operator *> OpenListEntry;

struct OpenListInfo {
    OpenListInfo(Heuristic *heur, bool only_pref);
    Heuristic *heuristic;
    bool only_preferred_operators;
    OpenList<OpenListEntry> open;
    int priority; // low value indicates high priority
};

class BestFirstSearchEngine : public SearchEngine {
    std::vector<Heuristic *> heuristics;
    std::vector<Heuristic *> preferred_operator_heuristics;
    std::vector<OpenListInfo> open_lists;
    ClosedList closed_list;

    std::vector<double> best_heuristic_values;
    int generated_states;

    TimeStampedState current_state;
    const TimeStampedState *current_predecessor;
    const Operator *current_operator;

    bool is_dead_end();
    bool check_goal();
    bool check_progress();
    void report_progress();
    void reward_progress();
    void generate_successors(const TimeStampedState *parent_ptr);
    void dump_transition() const;
    OpenListInfo *select_open_queue();
    
    void dump_plan_prefix_for_current_state() const;
    
    time_t last_time;
protected:
    virtual int step();
public:
    BestFirstSearchEngine();
    ~BestFirstSearchEngine();
    void add_heuristic(Heuristic *heuristic, bool use_estimates,
	    bool use_preferred_operators);
    virtual void statistics() const;
    virtual void initialize();
    int fetch_next_state();
};

#endif
