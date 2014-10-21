#ifndef TRANSITION_CACHE_H_
#define TRANSITION_CACHE_H_

#include "state.h"

#include <string>
// #include <map>
#include <ext/hash_map>
// using namespace std;
using namespace __gnu_cxx;

//class TransitionCache {
//    
//    struct OpHasher {
//	size_t operator()(const Operator &op) const {
//	    return hash<const char*>()(op.get_name().c_str());
//	}
//	bool operator()(const Operator &op1, const Operator &op2) const {
//	    return op1.get_name() == op2.get_name();
//	}
//    };
//    
//    struct TssHasher {
//	size_t operator()(const TimeStampedState &tss) const {
//	    size_t result = (tss.state.size() >= 1) ? static_cast<size_t>(tss.state[0] ): 1;
//	    for(int i = 1; i < tss.state.size(); i++)
//		result = g_variable_domain[i-1]*result + static_cast<size_t>(tss.state[i]);
//	    return result;
//	}
//	bool operator()(const TimeStampedState &tss1, const TimeStampedState &tss2) const {
//	    return !(tss1 < tss2) && !(tss2 < tss1); 
//	}
//    };
//
//    
//    typedef hash_map<Operator,const TimeStampedState*, OpHasher, OpHasher> SuccMap;
//    typedef hash_map<TimeStampedState,SuccMap, TssHasher, TssHasher> TransCache;
//    
//    TransCache cache;
//public:
//    TransitionCache();
//    const bool is_cached(const TimeStampedState& tss,
//	    const Operator& op) const;
//    const TimeStampedState& lookup(const TimeStampedState &tss,
//	    const Operator& op) const;
//    void store(const TimeStampedState &tss,
//	    const Operator& op, const TimeStampedState &succ);
//};
//
//class ConsistencyCache {
//    
//    struct TssHasher {
//	size_t operator()(const TimeStampedState &tss) const {    
//	    size_t result = (tss.state.size() >= 1) ? static_cast<size_t>(tss.state[0] ): 1;
//	    for(int i = 1; i < tss.state.size(); i++)
//		result = g_variable_domain[i-1]*result + static_cast<size_t>(tss.state[i]);
//	    result += static_cast<size_t>(tss.timestamp);
//	    for(int i = 0; i < tss.operators.size(); i++)
//		result += tss.operators[i].get_duration_var() + static_cast<size_t>(tss.operators[i].timepoint);
//	    result += tss.conds_at_end.size();
//	    result += tss.conds_over_all.size();
//	    result += tss.scheduled_effects.size();
//	    return result;
//	}
//	bool operator()(const TimeStampedState &tss1, const TimeStampedState &tss2) const {
//	    return !(tss1 < tss2) && !(tss2 < tss1); 
//	}
//    };
//    
//    typedef hash_map<TimeStampedState,bool,TssHasher,TssHasher> ConsCache;
//    ConsCache cache;
//public:
//    ConsistencyCache();
//    const bool is_cached(const TimeStampedState& tss) const;
//    const bool lookup(const TimeStampedState &tss) const;
//    void store(const TimeStampedState &tss, bool b);
//};




#endif /*TRANSITION_CACHE_H_*/
