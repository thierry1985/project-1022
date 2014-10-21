// HACK so that this will compile as a top-level target.
// This is to work around limitations of the Makefile wrt template stuff.

// #ifdef CLOSED_LIST_H
#include "closed_list.h"

// #include "state.h"

#include <algorithm>
#include <cassert>
using namespace std;

/*
 Map-based implementation of a closed list.
 Might be speeded up by using a hash_map, but then that's
 non-standard and requires defining hash keys.

 The closed list has two purposes:
 1. It stores which nodes have been expanded or scheduled to expand
 already to avoid duplicates (i.e., it is used like a set).
 These states "live" in the closed list -- in particular,
 permanently valid pointers to these states are obtained upon
 insertion.
 2. It can trace back a path from the initial state to a given state
 in the list.
 
 The datatypes used for the closed list could easily be
 parameterized, but there is no such need presently.
 */

ClosedList::ClosedList() {
}

ClosedList::~ClosedList() {
}

const TimeStampedState *ClosedList::insert(
	TimeStampedState &entry,
	const TimeStampedState *predecessor,
	const Operator *annotation) {
    
    
//    cout << "DEBUG: Adding to closed list (s,o,s'):" << endl;
//    if(predecessor) predecessor->dump(); else cout << "NULL" << endl;
//    if(annotation) annotation->dump(); else cout << "NULL" << endl;
//    entry.dump();
//    cout << endl << endl;
    
//    cout << "DEBUG: Call to ClosedList::insert(...)" << endl;
//    cout << "DEBUG: annotation is " << endl;
//    if(annotation) {
//	annotation->dump();
//    } else {
//	cout << "NULL" << endl;
//    }

    
    map<TimeStampedState, PredecessorInfo>::iterator it =
    closed.insert(make_pair(entry, PredecessorInfo(predecessor, annotation))).first;
    
//    cout << "DEBUG: closed_list size is " << closed.size() << endl;
    
    return &it->first;
}

void ClosedList::clear() {
    closed.clear();
}

bool ClosedList::contains(const TimeStampedState &entry) const {
    return closed.count(entry) != 0;
}

int ClosedList::size() const {
    return closed.size();
}

void ClosedList::trace_path(
	const TimeStampedState &entry, vector<PlanStep> &path) const {
    assert(path.empty());
    TimeStampedState current_entry = entry;
    for(;;) {
	const PredecessorInfo &info = closed.find(current_entry)->second;
	if(info.predecessor == 0)
	break;
	if(info.annotation != g_let_time_pass) {
	    const TimeStampedState* pred = info.predecessor;
	    const Operator* op = info.annotation;
	    path.push_back(PlanStep(pred->get_timestamp(),(*pred)[op->get_duration_var()],op));
	} else {
//	    cout << "DEBUG: In ClosedList::trace_path(...): info.annotation == NULL." << endl;
//	    cout << "DEBUG: info.predecessor = ";
//	    info.predecessor->dump();
//	    cout << "DEBUG: successor = ";
//	    current_entry.dump();
	}
	current_entry = *info.predecessor;
    }

    reverse(path.begin(), path.end());
}

// #endif
