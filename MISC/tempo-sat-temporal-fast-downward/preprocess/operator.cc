#include "helper_functions.h"
#include "operator.h"
#include "variable.h"

#include <cassert>
#include <iostream>
#include <fstream>
using namespace std;

Operator::Operator(istream &in, const vector<Variable *> &variables) {
    check_magic(in, "begin_operator");
    in >> ws;
    getline(in, name);
    int varNo;
    foperator fop;
    in >> fop >> varNo;
    duration_cond = DurationCond(fop, variables[varNo]);
    int count;
    variables[varNo]->set_used_in_duration_condition();
    in >> count; // number of prevail at-start conditions
    for(int i = 0; i < count; i++) {
	int varNo, val;
	in >> varNo >> val;
	prevail_start.push_back(Prevail(variables[varNo], val));
    }
    in >> count; // number of prevail overall conditions
    for(int i = 0; i < count; i++) {
	int varNo, val;
	in >> varNo >> val;
	prevail_overall.push_back(Prevail(variables[varNo], val));
    }
    in >> count; // number of prevail at-end conditions
    for(int i = 0; i < count; i++) {
	int varNo, val;
	in >> varNo >> val;
	prevail_end.push_back(Prevail(variables[varNo], val));
    }
    in >> count; // number of pre_post_start conditions
    for(int i = 0; i < count; i++) {
	int eff_conds;
	vector<EffCond> ecs_start;
	vector<EffCond> ecs_overall;
	vector<EffCond> ecs_end;
	in >> eff_conds;
	for(int j = 0; j < eff_conds; j++) {
	    int var, value;
	    in >> var >> value;
	    ecs_start.push_back(EffCond(variables[var], value));
	}
	in >> eff_conds;
	for(int j = 0; j < eff_conds; j++) {
	    int var, value;
	    in >> var >> value;
	    ecs_overall.push_back(EffCond(variables[var], value));
	}
	in >> eff_conds;
	for(int j = 0; j < eff_conds; j++) {
	    int var, value;
	    in >> var >> value;
	    ecs_end.push_back(EffCond(variables[var], value));
	}
	int varNo;
	in >> varNo;
	if(variables[varNo]->is_functional()) {
	    foperator assignop;
	    int varNo2;
	    in >> assignop >> varNo2;
	    if(eff_conds)
		numerical_effs_start.push_back(NumericalEffect(
			variables[varNo], ecs_start, ecs_overall, ecs_end,
			assignop, variables[varNo2]));
	    else
		numerical_effs_start.push_back(NumericalEffect(
			variables[varNo], assignop, variables[varNo2]));
	} else {
	    int val, newVal;
	    in >> val >> newVal;
	    if(eff_conds)
		pre_post_start.push_back(PrePost(variables[varNo], ecs_start,
			ecs_overall, ecs_end, val, newVal));
	    else
		pre_post_start.push_back(PrePost(variables[varNo], val, newVal));
	}
    }
    in >> count; // number of pre_post_end conditions
    for(int i = 0; i < count; i++) {
	int eff_conds;
	vector<EffCond> ecs_start;
	vector<EffCond> ecs_overall;
	vector<EffCond> ecs_end;
	in >> eff_conds;
	for(int j = 0; j < eff_conds; j++) {
	    int var, value;
	    in >> var >> value;
	    ecs_start.push_back(EffCond(variables[var], value));
	}
	in >> eff_conds;
	for(int j = 0; j < eff_conds; j++) {
	    int var, value;
	    in >> var >> value;
	    ecs_overall.push_back(EffCond(variables[var], value));
	}
	in >> eff_conds;
	for(int j = 0; j < eff_conds; j++) {
	    int var, value;
	    in >> var >> value;
	    ecs_end.push_back(EffCond(variables[var], value));
	}
	int varNo;
	in >> varNo;
	if(variables[varNo]->is_functional()) {
	    foperator assignop;
	    int varNo2;
	    in >> assignop >> varNo2;
	    if(eff_conds)
		numerical_effs_end.push_back(NumericalEffect(variables[varNo],
			ecs_start, ecs_overall, ecs_end, assignop,
			variables[varNo2]));
	    else
		numerical_effs_end.push_back(NumericalEffect(variables[varNo],
			assignop, variables[varNo2]));
	} else {
	    int val, newVal;
	    in >> val >> newVal;
	    if(eff_conds)
		pre_post_end.push_back(PrePost(variables[varNo], ecs_start,
			ecs_overall, ecs_end, val, newVal));
	    else
		pre_post_end.push_back(PrePost(variables[varNo], val, newVal));
	}
    }
    check_magic(in, "end_operator");
    // TODO: Evtl. effektiver: conditions schon sortiert einlesen?
}

void Operator::dump() const {
    cout << name << ":" << endl;
    cout << "duration:" << duration_cond.op << " "
	    << duration_cond.var->get_name();
    cout << endl;
    cout << "prevail (at-start):";
    for(int i = 0; i < prevail_start.size(); i++)
	cout << "  " << prevail_start[i].var->get_name() << " := "
		<< prevail_start[i].prev;
    cout << endl;
    cout << "prevail (overall):";
    for(int i = 0; i < prevail_overall.size(); i++)
	cout << "  " << prevail_overall[i].var->get_name() << " := "
		<< prevail_overall[i].prev;
    cout << endl;
    cout << "prevail (at-end):";
    for(int i = 0; i < prevail_end.size(); i++)
	cout << "  " << prevail_end[i].var->get_name() << " := "
		<< prevail_end[i].prev;
    cout << endl;
    cout << "pre-post (at-start):";
    for(int i = 0; i < pre_post_start.size(); i++) {
	if(pre_post_start[i].is_conditional_effect) {
	    cout << "  if at-start(";
	    for(int j = 0; j < pre_post_start[i].effect_conds_start.size(); j++)
		cout << pre_post_start[i].effect_conds_start[j].var->get_name()
			<< " := "
			<< pre_post_start[i].effect_conds_start[j].cond;
	    cout << ")";
	    cout << "  if overall(";
	    for(int j = 0; j < pre_post_start[i].effect_conds_overall.size(); j++)
		cout
			<< pre_post_start[i].effect_conds_overall[j].var->get_name()
			<< " := "
			<< pre_post_start[i].effect_conds_overall[j].cond;
	    cout << ")";
	    cout << "  if at-end(";
	    for(int j = 0; j < pre_post_start[i].effect_conds_end.size(); j++)
		cout << pre_post_start[i].effect_conds_end[j].var->get_name()
			<< " := " << pre_post_start[i].effect_conds_end[j].cond;
	    cout << ")";
	}
	cout << " " << pre_post_start[i].var->get_name() << " : "
		<< pre_post_start[i].pre <<" -> "<< pre_post_start[i].post;
    }
    cout << endl;
    cout << "pre-post (at-end):";
    for(int i = 0; i < pre_post_end.size(); i++) {
	if(pre_post_end[i].is_conditional_effect) {
	    cout << "  if at-start(";
	    for(int j = 0; j < pre_post_end[i].effect_conds_start.size(); j++)
		cout << pre_post_end[i].effect_conds_start[j].var->get_name()
			<< " := " << pre_post_end[i].effect_conds_start[j].cond;
	    cout << ")";
	    cout << "  if overall(";
	    for(int j = 0; j < pre_post_end[i].effect_conds_overall.size(); j++)
		cout << pre_post_end[i].effect_conds_overall[j].var->get_name()
			<< " := "
			<< pre_post_end[i].effect_conds_overall[j].cond;
	    cout << ")";
	    cout << "  if at-end(";
	    for(int j = 0; j < pre_post_end[i].effect_conds_end.size(); j++)
		cout << pre_post_end[i].effect_conds_end[j].var->get_name()
			<< " := " << pre_post_end[i].effect_conds_end[j].cond;
	    cout << ")";
	}
	cout << " " << pre_post_end[i].var->get_name() << " : "
		<< pre_post_end[i].pre <<" -> "<< pre_post_end[i].post;
    }
    cout << endl;
    cout << "numerical effects (at-start):";
    for(int i = 0; i< numerical_effs_start.size(); i++) {
	if(numerical_effs_start[i].is_conditional_effect) {
	    cout << "  if at-start(";
	    for(int j = 0; j
		    < numerical_effs_start[i].effect_conds_start.size(); j++)
		cout
			<< numerical_effs_start[i].effect_conds_start[j].var->get_name()
			<< " := "
			<< numerical_effs_start[i].effect_conds_start[j].cond;
	    cout << ")";
	    cout << "  if overall(";
	    for(int j = 0; j
		    < numerical_effs_start[i].effect_conds_overall.size(); j++)
		cout
			<< numerical_effs_start[i].effect_conds_overall[j].var->get_name()
			<< " := "
			<< numerical_effs_start[i].effect_conds_overall[j].cond;
	    cout << ")";
	    cout << "  if at-end(";
	    for(int j = 0; j
		    < numerical_effs_start[i].effect_conds_end.size(); j++)
		cout
			<< numerical_effs_start[i].effect_conds_end[j].var->get_name()
			<< " := "
			<< numerical_effs_start[i].effect_conds_end[j].cond;
	    cout << ")";

	}
	cout << " " << numerical_effs_start[i].var->get_name() << " "
		<< numerical_effs_start[i].fop << " "
		<< numerical_effs_start[i].foperand->get_name();
    }
    cout << endl;
    cout << "numerical effects (at-end):";
    for(int i = 0; i< numerical_effs_end.size(); i++) {
	if(numerical_effs_end[i].is_conditional_effect) {
	    cout << "  if at-start(";
	    for(int j = 0; j < numerical_effs_end[i].effect_conds_start.size(); j++)
		cout
			<< numerical_effs_end[i].effect_conds_start[j].var->get_name()
			<< " := "
			<< numerical_effs_end[i].effect_conds_start[j].cond;
	    cout << ")";
	    cout << "  if overall(";
	    for(int j = 0; j
		    < numerical_effs_end[i].effect_conds_overall.size(); j++)
		cout
			<< numerical_effs_end[i].effect_conds_overall[j].var->get_name()
			<< " := "
			<< numerical_effs_end[i].effect_conds_overall[j].cond;
	    cout << ")";
	    cout << "  if at-end(";
	    for(int j = 0; j < numerical_effs_end[i].effect_conds_end.size(); j++)
		cout
			<< numerical_effs_end[i].effect_conds_end[j].var->get_name()
			<< " := "
			<< numerical_effs_end[i].effect_conds_end[j].cond;
	    cout << ")";
	}
	cout << " " << numerical_effs_end[i].var->get_name() << " "
		<< numerical_effs_end[i].fop << " "
		<< numerical_effs_end[i].foperand->get_name();
    }
    cout << endl;
}

void Operator::strip_unimportant_effects() {
    int new_index = 0;
    for(int i = 0; i < pre_post_start.size(); i++) {
	if(pre_post_start[i].var->get_level() != -1)
	    pre_post_start[new_index++] = pre_post_start[i];
    }
    pre_post_start.erase(pre_post_start.begin() + new_index,
	    pre_post_start.end());
    new_index = 0;
    for(int i = 0; i < pre_post_end.size(); i++) {
	if(pre_post_end[i].var->get_level() != -1)
	    pre_post_end[new_index++] = pre_post_end[i];
    }
    pre_post_end.erase(pre_post_end.begin() + new_index, pre_post_end.end());
    //TODO: do the same for numeric effects!
}

bool Operator::is_redundant() const {
    return (pre_post_start.empty()&&pre_post_end.empty()
	    &&numerical_effs_start.empty()&&numerical_effs_end.empty());//TODO: Is this correct?
}

void strip_operators(vector<Operator> &operators) {
    int old_count = operators.size();
    int new_index = 0;
    for(int i = 0; i < operators.size(); i++) {
	operators[i].strip_unimportant_effects();
	if(!operators[i].is_redundant())
	    operators[new_index++] = operators[i];
    }
    operators.erase(operators.begin() + new_index, operators.end());
    cout << operators.size() << " of " << old_count << " operators necessary."
	    << endl;
}

void Operator::generate_cpp_input(ofstream &outfile) const {
    //TODO: beim Einlesen in search feststellen, ob leerer Operator
    outfile << "begin_operator" << endl;
    outfile << name << endl;
    //duration
    outfile << duration_cond.op << " " << duration_cond.var->get_level()
	    << endl;
    //prevails
    outfile << prevail_start.size() << endl;
    for(int i = 0; i < prevail_start.size(); i++) {
	assert(prevail_start[i].var->get_level() != -1);
	if(prevail_start[i].var->get_level() != -1)
	    outfile << prevail_start[i].var->get_level() << " "
		    << prevail_start[i].prev << endl;
    }
    outfile << prevail_overall.size() << endl;
    for(int i = 0; i < prevail_overall.size(); i++) {
	assert(prevail_overall[i].var->get_level() != -1);
	if(prevail_overall[i].var->get_level() != -1)
	    outfile << prevail_overall[i].var->get_level() << " "
		    << prevail_overall[i].prev << endl;
    }
    outfile << prevail_end.size() << endl;
    for(int i = 0; i < prevail_end.size(); i++) {
	assert(prevail_end[i].var->get_level() != -1);
	if(prevail_end[i].var->get_level() != -1)
	    outfile << prevail_end[i].var->get_level() << " "
		    << prevail_end[i].prev << endl;
    }

    //pre_posts
    outfile << pre_post_start.size() << endl;
    for(int i = 0; i < pre_post_start.size(); i++) {
	assert(pre_post_start[i].var->get_level() != -1);
	if(pre_post_start[i].effect_conds_start.size() > 0) {
	    outfile << pre_post_start[i].effect_conds_start.size() << endl;
	    for(int j = 0; j < pre_post_start[i].effect_conds_start.size(); j++)
		outfile
			<< pre_post_start[i].effect_conds_start[j].var->get_level()
			<< " " << pre_post_start[i].effect_conds_start[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	if(pre_post_start[i].effect_conds_overall.size() > 0) {
	    outfile << pre_post_start[i].effect_conds_overall.size() << endl;
	    for(int j = 0; j < pre_post_start[i].effect_conds_overall.size(); j++)
		outfile
			<< pre_post_start[i].effect_conds_overall[j].var->get_level()
			<< " "
			<< pre_post_start[i].effect_conds_overall[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	if(pre_post_start[i].effect_conds_end.size() > 0) {
	    outfile << pre_post_start[i].effect_conds_end.size() << endl;
	    for(int j = 0; j < pre_post_start[i].effect_conds_end.size(); j++)
		outfile
			<< pre_post_start[i].effect_conds_end[j].var->get_level()
			<< " " << pre_post_start[i].effect_conds_end[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	outfile << pre_post_start[i].var->get_level() << " "
		<< pre_post_start[i].pre << " " << pre_post_start[i].post
		<< endl;
    }
    outfile << pre_post_end.size() << endl;
    for(int i = 0; i < pre_post_end.size(); i++) {
	assert(pre_post_end[i].var->get_level() != -1);
	if(pre_post_end[i].effect_conds_start.size()>0) {
	    outfile << pre_post_end[i].effect_conds_start.size() << endl;
	    for(int j = 0; j < pre_post_end[i].effect_conds_start.size(); j++)
		outfile
			<< pre_post_end[i].effect_conds_start[j].var->get_level()
			<< " " << pre_post_end[i].effect_conds_start[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	if(pre_post_end[i].effect_conds_overall.size()>0) {
	    outfile << pre_post_end[i].effect_conds_overall.size() << endl;
	    for(int j = 0; j < pre_post_end[i].effect_conds_overall.size(); j++)
		outfile
			<< pre_post_end[i].effect_conds_overall[j].var->get_level()
			<< " " << pre_post_end[i].effect_conds_overall[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	if(pre_post_end[i].effect_conds_end.size() > 0) {
	    outfile << pre_post_end[i].effect_conds_end.size() << endl;
	    for(int j = 0; j < pre_post_end[i].effect_conds_end.size(); j++)
		outfile << pre_post_end[i].effect_conds_end[j].var->get_level()
			<< " " << pre_post_end[i].effect_conds_end[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	outfile << pre_post_end[i].var->get_level() << " "
		<< pre_post_end[i].pre << " " << pre_post_end[i].post << endl;
    }
    outfile << numerical_effs_start.size() << endl;
    for(int i = 0; i < numerical_effs_start.size(); i++) {
	assert(numerical_effs_start[i].var->get_level() != -1);
	if(numerical_effs_start[i].effect_conds_start.size() > 0) {
	    outfile << numerical_effs_start[i].effect_conds_start.size()
		    << endl;
	    for(int j = 0; j
		    < numerical_effs_start[i].effect_conds_start.size(); j++)
		outfile
			<< numerical_effs_start[i].effect_conds_start[j].var->get_level()
			<< " "
			<< numerical_effs_start[i].effect_conds_start[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	if(numerical_effs_start[i].effect_conds_overall.size() > 0) {
	    outfile << numerical_effs_start[i].effect_conds_overall.size()
		    << endl;
	    for(int j = 0; j
		    < numerical_effs_start[i].effect_conds_overall.size(); j++)
		outfile
			<< numerical_effs_start[i].effect_conds_overall[j].var->get_level()
			<< " "
			<< numerical_effs_start[i].effect_conds_overall[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	if(numerical_effs_start[i].effect_conds_end.size() > 0) {
	    outfile << numerical_effs_start[i].effect_conds_end.size() << endl;
	    for(int j = 0; j < numerical_effs_start[i].effect_conds_end.size(); j++)
		outfile
			<< numerical_effs_start[i].effect_conds_end[j].var->get_level()
			<< " "
			<< numerical_effs_start[i].effect_conds_end[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	outfile << numerical_effs_start[i].var->get_level() << " "
		<< numerical_effs_start[i].fop << " "
		<< numerical_effs_start[i].foperand->get_level() << endl;
    }
    outfile << numerical_effs_end.size() << endl;
    for(int i = 0; i < numerical_effs_end.size(); i++) {
	assert(numerical_effs_end[i].var->get_level() != -1);
	if(numerical_effs_end[i].effect_conds_start.size() > 0) {
	    outfile << numerical_effs_end[i].effect_conds_start.size() << endl;
	    for(int j = 0; j < numerical_effs_end[i].effect_conds_start.size(); j++)
		outfile
			<< numerical_effs_end[i].effect_conds_start[j].var->get_level()
			<< " "
			<< numerical_effs_end[i].effect_conds_start[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	if(numerical_effs_end[i].effect_conds_overall.size() > 0) {
	    outfile << numerical_effs_end[i].effect_conds_overall.size()
		    << endl;
	    for(int j = 0; j
		    < numerical_effs_end[i].effect_conds_overall.size(); j++)
		outfile
			<< numerical_effs_end[i].effect_conds_overall[j].var->get_level()
			<< " "
			<< numerical_effs_end[i].effect_conds_overall[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	if(numerical_effs_end[i].effect_conds_end.size() > 0) {
	    outfile << numerical_effs_end[i].effect_conds_end.size() << endl;
	    for(int j = 0; j < numerical_effs_end[i].effect_conds_end.size(); j++)
		outfile
			<< numerical_effs_end[i].effect_conds_end[j].var->get_level()
			<< " "
			<< numerical_effs_end[i].effect_conds_end[j].cond
			<< endl;
	} else {
	    outfile << "0" << endl;
	}
	outfile << numerical_effs_end[i].var->get_level() << " "
		<< numerical_effs_end[i].fop << " "
		<< numerical_effs_end[i].foperand->get_level() << endl;
    }
    outfile << "end_operator" << endl;
}
