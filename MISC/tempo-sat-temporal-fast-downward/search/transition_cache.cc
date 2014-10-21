#include "transition_cache.h"

// using namespace std;
using namespace __gnu_cxx;

//TransitionCache::TransitionCache() {}
//
//const bool TransitionCache::is_cached(const TimeStampedState& tss,
//	    const Operator& op) const {    
//    TransCache::const_iterator it = cache.find(tss);
//    if(it == cache.end()) return false;
//    
//    SuccMap::const_iterator it2 = (it->second).find(op);
//    if(it2 == (it->second).end()) return false;
//    
//    return true;
//}
//
//const TimeStampedState& TransitionCache::lookup(const TimeStampedState &tss,
//	    const Operator& op) const {
//    assert(is_cached(tss,op));
//    TransCache::const_iterator it = cache.find(tss);
//    SuccMap::const_iterator it2 = (it->second).find(op);
//    return *(it2->second);
//}
//
//void TransitionCache::store(const TimeStampedState &tss,
//	    const Operator& op, const TimeStampedState &succ) {
//    cache[tss][op] = &succ;
//}
//
//ConsistencyCache::ConsistencyCache() {}
//
//const bool ConsistencyCache::is_cached(const TimeStampedState& tss) const {
//    ConsCache::const_iterator it = cache.find(tss);
//    return (it != cache.end());
//}
//
//const bool ConsistencyCache::lookup(const TimeStampedState &tss) const {
//    assert(is_cached(tss));
//    ConsCache::const_iterator it = cache.find(tss);
//    return it->second;
//}
//
//void ConsistencyCache::store(const TimeStampedState &tss, bool b) {
//        cache[tss] = b;
//}
