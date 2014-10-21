#ifndef CLOSED_LIST_H
#define CLOSED_LIST_H

#include <map>
#include <vector>

#include "operator.h"
// #include "search_engine.h"

// class Operator;
// class TimeStampedState;
// struct PlanStep;

class ClosedList {
    struct PredecessorInfo {
	const TimeStampedState *predecessor;
	const Operator *annotation;
	PredecessorInfo(const TimeStampedState *pred,
		const Operator *annote) :
	    predecessor(pred), annotation(annote) {
	}
    };

    struct TssCompareIgnoreTimestamps {
	
	bool operator()(const TimeStampedState &tss1, const TimeStampedState &tss2) const {
	    //	    if(tss1.timestamp < tss2.timestamp) return true;
	    //	    if(tss1.timestamp > tss2.timestamp) return false;
	    if(lexicographical_compare(tss1.state.begin(), tss1.state.end(),
		    tss2.state.begin(), tss2.state.end())) return true;
	    if(lexicographical_compare(tss2.state.begin(), tss2.state.end(),
		    tss1.state.begin(), tss1.state.end())) return false;
	    if(lexicographical_compare(tss1.dirty_bits.begin(), tss1.dirty_bits.end(),
		    tss2.dirty_bits.begin(), tss2.dirty_bits.end())) return true;
	    if(lexicographical_compare(tss2.dirty_bits.begin(), tss2.dirty_bits.end(),
		    tss1.dirty_bits.begin(), tss1.dirty_bits.end())) return false;
	    if(tss1.scheduled_effects.size() < tss2.scheduled_effects.size()) return true;
	    if(tss1.scheduled_effects.size() > tss2.scheduled_effects.size()) return false;
	    if(tss1.conds_over_all.size() < tss2.conds_over_all.size()) return true;
	    if(tss1.conds_over_all.size() > tss2.conds_over_all.size()) return false;
	    if(tss1.conds_at_end.size() < tss2.conds_at_end.size()) return true;
	    if(tss1.conds_at_end.size() > tss2.conds_at_end.size()) return false;
	    if(lexicographical_compare(tss1.scheduled_effects.begin(), tss1.scheduled_effects.end(),
		    tss2.scheduled_effects.begin(), tss2.scheduled_effects.end())) return true;
	    if(lexicographical_compare(tss2.scheduled_effects.begin(), tss2.scheduled_effects.end(),
		    tss1.scheduled_effects.begin(), tss1.scheduled_effects.end())) return false;
	    if(lexicographical_compare(tss1.conds_over_all.begin(), tss1.conds_over_all.end(),
		    tss2.conds_over_all.begin(), tss2.conds_over_all.end())) return true;
	    if(lexicographical_compare(tss2.conds_over_all.begin(), tss2.conds_over_all.end(),
		    tss1.conds_over_all.begin(), tss1.conds_over_all.end())) return false;
	    if(lexicographical_compare(tss1.conds_at_end.begin(), tss1.conds_at_end.end(),
		    tss2.conds_at_end.begin(), tss2.conds_at_end.end())) return true;
	    if(lexicographical_compare(tss2.conds_at_end.begin(), tss2.conds_at_end.end(),
		    tss1.conds_at_end.begin(), tss1.conds_at_end.end())) return false;
	    return false;
	}
    };
    
    typedef std::map<TimeStampedState, PredecessorInfo, TssCompareIgnoreTimestamps> ClosedListMap;
    // std::map<Entry, PredecessorInfo> closed;
    ClosedListMap closed;
public:
    ClosedList();
    ~ClosedList();
    const TimeStampedState *insert(TimeStampedState &entry,
	    const TimeStampedState *predecessor,
	    const Operator *annotation);
    void clear();

    bool contains(const TimeStampedState &entry) const;
    
    const TimeStampedState& get(const TimeStampedState &state) const {
	return closed.find(state)->first;
    }
    
    int size() const;
    void trace_path(const TimeStampedState &entry, std::vector<PlanStep> &path) const;
};

// #include "closed_list.cc" // HACK! Templates and the current Makefile don't mix well
#endif
