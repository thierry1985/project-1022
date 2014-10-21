#include "relaxed_state.h"

#include "state.h"
#include "causal_graph.h"
#include "domain_transition_graph.h"

#include <vector>
#include <cassert>

using namespace std;

Interval& Interval::assign(Interval const& other) {
    this->left_point = min(this->left_point, other.left_point);
    this->right_point = max(this->right_point, other.right_point);
    return *this;
}

Interval& Interval::increase(Interval const& other) {
    this->left_point = min(this->left_point, (this->left_point
	    + other.left_point));
    this->right_point = max(this->right_point, (this->right_point
	    + other.right_point));
    return *this;
}

Interval& Interval::decrease(Interval const& other) {
    this->left_point = min(this->left_point, (this->left_point
	    - other.right_point));
    this->right_point = max(this->right_point, (this->right_point
	    - other.left_point));
    return *this;
}

Interval& Interval::scale_up(Interval const& other) {
    double new_left_point, new_right_point;
    double tmp_point;
    tmp_point = left_point * other.left_point;
    new_left_point = tmp_point;
    new_right_point = tmp_point;
    tmp_point = left_point * other.right_point;
    if(tmp_point < new_left_point)
	new_left_point = tmp_point;
    if(tmp_point> new_right_point)
	new_right_point = tmp_point;
    tmp_point = right_point * other.left_point;
    if(tmp_point < new_left_point)
	new_left_point = tmp_point;
    if(tmp_point> new_right_point)
	new_right_point = tmp_point;
    tmp_point = right_point * other.right_point;
    if(tmp_point < new_left_point)
	new_left_point = tmp_point;
    if(tmp_point> new_right_point)
	new_right_point = tmp_point;
    left_point = min(left_point,new_left_point);
    right_point = max(right_point,new_right_point);
    return *this;
}

Interval& Interval::scale_down(Interval const& other) {
    //TODO: HACK ?
    double new_left_point, new_right_point;
    if(left_point >= 0) {
	if(other.left_point> 0) {
	    new_left_point = left_point / other.right_point;
	    new_right_point = right_point / other.left_point;
	} else if(other.right_point < 0) {
	    new_left_point = right_point / other.right_point;
	    new_right_point = left_point / other.left_point;
	} else {
	    new_left_point = REALLYSMALL;
	    new_right_point = REALLYBIG;
	}
    } else if(right_point <= 0) {
	if(other.left_point> 0) {
	    new_left_point = left_point / other.left_point;
	    new_right_point = right_point / other.right_point;
	} else if(other.right_point < 0) {
	    new_left_point = right_point / other.left_point;
	    new_right_point = left_point / other.right_point;
	} else {
	    new_left_point = REALLYSMALL;
	    new_right_point = REALLYBIG;
	}
    } else {
	new_left_point = REALLYSMALL;
	new_right_point = REALLYBIG;
    }
    left_point = min(left_point, new_left_point);
    right_point = max(right_point, new_right_point);
    return *this;
}

Interval const add_intervals(Interval const& left, Interval const& right) {
    return Interval((left.left_point + right.left_point), (left.right_point
	    + right.right_point));
}

Interval const subtract_intervals(Interval const& left, Interval const& right) {
    double new_left_point = min((left.left_point - right.left_point),
	    (left.left_point - right.right_point));
    double new_right_point = max((left.right_point - right.left_point),
	    (left.right_point - right.right_point));
    return Interval(new_left_point, new_right_point);
}

Interval const multiply_intervals(Interval const& left, Interval const& right) {
    double new_left_point, new_right_point;
    double tmp_point;
    tmp_point = left.left_point * right.left_point;
    new_left_point = tmp_point;
    new_right_point = tmp_point;
    tmp_point = left.left_point * right.right_point;
    if(tmp_point < new_left_point)
	new_left_point = tmp_point;
    if(tmp_point > new_right_point)
	new_right_point = tmp_point;
    tmp_point = left.right_point * right.left_point;
    if(tmp_point < new_left_point)
	new_left_point = tmp_point;
    if(tmp_point > new_right_point)
	new_right_point = tmp_point;
    tmp_point = left.right_point * right.right_point;
    if(tmp_point < new_left_point)
	new_left_point = tmp_point;
    if(tmp_point > new_right_point)
	new_right_point = tmp_point;
    return Interval(new_left_point, new_right_point);
}

Interval const divide_intervals(Interval const& left, Interval const& right) {
    double new_left_point, new_right_point;
    if(left.left_point >= 0) {
	if(right.left_point > 0) {
	    new_left_point = left.left_point / right.right_point;
	    new_right_point = left.right_point / right.left_point;
	} else if(right.right_point < 0) {
	    new_left_point = left.right_point / right.right_point;
	    new_right_point = left.left_point / right.left_point;
	} else { //TODO: divison by 0!
	    new_left_point = REALLYSMALL;
	    new_right_point = REALLYBIG;
	}
    } else if(left.right_point <= 0) {
	if(right.left_point > 0) {
	    new_left_point = left.left_point / right.left_point;
	    new_right_point = left.right_point / right.right_point;
	} else if(right.right_point < 0) {
	    new_left_point = left.right_point / right.left_point;
	    new_right_point = left.left_point / right.right_point;
	} else { //TODO: divison by 0!
	    new_left_point = REALLYSMALL;
	    new_right_point = REALLYBIG;
	}
    } else {
	new_left_point = REALLYSMALL;
	new_right_point = REALLYBIG;
    }
    return Interval(new_left_point, new_right_point);
}

Interval intersect(Interval a, Interval b) {
    return Interval(min(a.left_point, b.left_point), max(a.right_point,
	    b.right_point));
}

bool Interval::operator<(const Interval& other) const {
    if(left_point < other.left_point)
	return true;
    else if(other.left_point < left_point)
	return false;
    else if(right_point < other.right_point)
	return true;
    else return false;
}

RelaxedState::RelaxedState(const TimeStampedState &tss) {
    intervals.resize(g_variable_domain.size());
    values.resize(g_variable_domain.size());
    //initialize with current state values
    for(int i = 0; i < g_variable_domain.size(); i++) {
	if(g_variable_types[i] == logical || g_variable_types[i] == comparison) {
	    values[i].push_front(static_cast<int>(tss.state[i]));
	} else { //subterm || primitive_functional
	    intervals[i].left_point = tss.state[i];
	    intervals[i].right_point = tss.state[i];
	}
    }
    //break down scheduled effects (ignore effect conditions)
    for(int i = 0; i < tss.scheduled_effects.size(); i++) {
	ScheduledEffect se = tss.scheduled_effects[i];
	int var_no = se.var;
	if(g_variable_types[var_no] == logical || g_variable_types[var_no]
		== comparison) {
	    values[var_no].push_front(static_cast<int>(se.post));
	} else { //subterm || primitive_functional
	    updatePrimitiveNumericVariables(se.fop, se.var, se.var_post);
	}
    }
}

void RelaxedState::updateNumericValuesRec(int var) {
    vector<int> depending = g_causal_graph->get_successors(var);
    for(int i = 0; i < depending.size(); i++) {
	if(var < depending[i]) { //ignore cycels
	    DomainTransitionGraph *dtg = g_transition_graphs[depending[i]];
	    if(g_variable_types[depending[i]] == subterm_functional) {
		DomainTransitionGraphSubterm *dtgs =
			dynamic_cast<DomainTransitionGraphSubterm*>(dtg);
		assert(dtgs->get_left_var() == var || dtgs->get_right_var() == var);
		updateSubtermNumericVariables(depending[i], dtgs->get_op(),
			dtgs->get_left_var(), dtgs->get_right_var());
	    } else if(g_variable_types[depending[i]] == comparison) {
		DomainTransitionGraphComp *dtgc =
			dynamic_cast<DomainTransitionGraphComp*>(dtg);
		assert(dtgc->nodes.first.left_var == var || dtgc->nodes.first.right_var == var);
		updateCompNumericVariables(depending[i],
			dtgc->nodes.first.op,
			dtgc->nodes.first.left_var,
			dtgc->nodes.first.right_var);
	    }
	}
    }
}

void RelaxedState::updatePrimitiveNumericVariables(assignment_op op,
	int left_var, int right_var) {
    Interval &left_interval = intervals[left_var];
    Interval &right_interval = intervals[right_var];
    switch (op) {
    case assign:
	left_interval.assign(right_interval);
	break;
    case scale_up:
	left_interval.scale_up(right_interval);
	break;
    case scale_down:
	left_interval.scale_down(right_interval);
	break;
    case increase:
	left_interval.increase(right_interval);
	break;
    case decrease:
	left_interval.decrease(right_interval);
	break;
    default:
	assert(false);
	break;
    }
    updateNumericValuesRec(left_var);
}

void RelaxedState::updateCompNumericVariables(int var, binary_op op,
	int left_var, int right_var) {
    Interval &left = intervals[left_var];
    Interval &right = intervals[right_var];
    __gnu_cxx::slist<int> target = values[var];

    switch (op) {
    case lt:
	if(left.left_point < right.right_point) {
	    target.push_front(1);
	}
	if(left.right_point >= right.left_point) {
	    target.push_front(0);
	}
	break;
    case le:
	if(left.left_point <= right.right_point) {
	    target.push_front(1);
	}
	if(left.right_point > right.left_point) {
	    target.push_front(0);
	}
	break;
    case eq:
	if(!(left.right_point < right.left_point) && !(left.left_point
		> right.right_point)) {
	    target.push_front(1);
	}
	if(!(double_equals(left.left_point,left.right_point) && double_equals(right.left_point,
		right.right_point) && double_equals(left.left_point,right.left_point))) {
	    target.push_front(0);
	}
	break;
    case ge:
	if(left.right_point >= right.left_point) {
	    target.push_front(1);
	}
	if(left.left_point < right.right_point) {
	    target.push_front(0);
	}
	break;
    case gt:
	if(left.right_point > right.left_point) {
	    target.push_front(1);
	}
	if(left.left_point <= right.right_point) {
	    target.push_front(0);
	}
	break;
    case ue:
	if(!(left.right_point < right.left_point) && !(left.left_point
		> right.right_point)) {
	    target.push_front(0);
	}
	if(!(double_equals(left.left_point,left.right_point) && double_equals(right.left_point,
		right.right_point) && double_equals(left.left_point,right.left_point))) {
	    target.push_front(1);
	}
	break;
    default:
	assert(false);
	break;
    }
    updateNumericValuesRec(var);
}

void RelaxedState::updateSubtermNumericVariables(int var, binary_op op,
	int left_var, int right_var) {
    Interval &left = intervals[left_var];
    Interval &right = intervals[right_var];
    Interval &target = intervals[var];
    switch (op) {
    case add:
	target = intersect(target, add_intervals(left, right));
	break;
    case subtract:
	target = intersect(target, subtract_intervals(left, right));
	break;
    case mult:
	target = intersect(target, multiply_intervals(left, right));
	break;
    case divis:
	target = intersect(target, divide_intervals(left, right));
	break;
    default:
	assert(false);
	break;
    }
    updateNumericValuesRec(var);
}

const Interval &RelaxedState::getIntervalOfVariable(const int var) const {
    return intervals[var];
}

const __gnu_cxx::slist<int> &RelaxedState::getValuesOfVariable(const int var) const {
    return values[var];
}

void RelaxedState::dump() const {
    for(int i = 0; i < g_variable_domain.size(); i++) {
	cout << i << ": ";
	if(g_variable_types[i] == logical || g_variable_types[i] == comparison) {
	    __gnu_cxx::slist<int>::const_iterator it;
	    for(it = values[i].begin(); it != values[i].end(); ++it) {
		cout << *it << " ";
	    }
	    cout << endl;
	} else { //subterm || primitive_functional
	    cout << "[" << intervals[i].left_point << ","
		    << intervals[i].right_point << "]" << endl;
	}
    }
}
