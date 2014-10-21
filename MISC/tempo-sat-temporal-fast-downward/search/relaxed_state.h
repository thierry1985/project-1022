#ifndef RELAXED_STATE_H_
#define RELAXED_STATE_H_

#include "globals.h"

#include <ext/slist>
#include <vector>
using namespace std;

class Interval {
public:
    Interval(double lp, double rp) {
	left_point = lp;
	right_point = rp;
    }
    Interval(double p) {
	left_point = p;
	right_point = p;
    }
    Interval() {
	left_point = 0.0;
	right_point = 0.0;
    }
    double left_point;
    double right_point;

    Interval& assign (Interval const& other);
    Interval& increase (Interval const& other);
    Interval& decrease (Interval const& other);
    Interval& scale_up (Interval const& other);
    Interval& scale_down (Interval const& other);
    
    bool operator< (const Interval& other) const;
};

Interval const add_intervals (Interval const& left, Interval const& right);
Interval const subtract_intervals (Interval const& left, Interval const& right);
Interval const multiply_intervals (Interval const& left, Interval const& right);
Interval const divide_intervals (Interval const& left, Interval const& right);

Interval intersect(Interval a, Interval b);

class RelaxedState {
private:
    vector<Interval> intervals;
    vector<__gnu_cxx::slist<int> > values;
    void updatePrimitiveNumericVariables(assignment_op op, int left_variable,
	    int right_variable);
    void updateSubtermNumericVariables(int var, binary_op op,
	    int left_variable, int right_variable);
    void updateCompNumericVariables(int var, binary_op op, int left_variable,
	    int right_variable);
    void updateNumericValuesRec(int var);
public:
    RelaxedState(const TimeStampedState &tss);
    void dump() const;
    const Interval &getIntervalOfVariable(const int var) const;
    const __gnu_cxx::slist<int> &getValuesOfVariable(const int var) const;
};

#endif /*RELAXED_STATE_H_*/
