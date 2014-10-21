#include "best_first_search.h"
#include "cg_heuristic.h"
#include "cyclic_cg_heuristic.h"
#include "no_heuristic.h"

#include "globals.h"
#include "operator.h"
#include "relaxed_state.h"
#include "transition_cache.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

#include <cstdio>
#include <math.h>

using namespace std;

#include <sys/times.h>

double save_plan(const vector<PlanStep> &plan, double best_makespan, int &plan_number, string &plan_name);

int main(int argc, const char **argv) {
    ifstream file("../preprocess/output");
    if(strcmp(argv[argc-1], "-eclipserun")==0) {
	cin.rdbuf(file.rdbuf());
	cerr.rdbuf(cout.rdbuf());
	argc--;
    }

    struct tms start, search_start, search_end;
    times(&start);
    bool poly_time_method = false;

    g_greedy = false;
    bool cg_heuristic = false, cg_preferred_operators = false;
    bool cyclic_cg_heuristic = false, cyclic_cg_preferred_operators = false;
    bool no_heuristic = false;
    string plan_name = "sas_plan";
    //    bool ff_heuristic = false, ff_preferred_operators = false;
    //    bool goal_count_heuristic = false;
    //    bool blind_search_heuristic = false;
    if(argc == 1) {
	cerr << "missing argument: output file name" << endl;
	return 1;
    } else {
	plan_name = string(argv[argc - 1]);
	argc--;
    }

    for(int i = 1; i < argc; i++) {
	for(const char *c = argv[i]; *c != 0; c++) {
	    if(*c == 'g') {
	    g_greedy = true;
	    } else if(*c == 'c') {
		cg_heuristic = true;
	    } else if(*c == 'C') {
		cg_preferred_operators = true;
	    } else if(*c == 'y') {
		cyclic_cg_heuristic = true;
	    } else if(*c == 'Y') {
		cyclic_cg_preferred_operators = true;
	    } else if(*c == 'n') {
		no_heuristic = true;
	    } else {
		cerr << "Unknown option: " << *c << endl;
		return 1;
	    }
	}
    }

    if(!cg_heuristic && !cyclic_cg_heuristic && !no_heuristic) {
	cerr << "Error: you must select at least one heuristic!" << endl
		<< "If you are unsure, choose options \"yY\"." << endl;
	return 2;
    }

    cin >> poly_time_method;
    if(poly_time_method) {
	cout << "Poly-time method not implemented in this branch." << endl;
	cout << "Starting normal solver." << endl;
	// cout << "Aborting." << endl;
	// return 1;
    }

    read_everything(cin);
    g_let_time_pass = new Operator();
//    g_consistency_cache = new ConsistencyCache();

//    dump_DTGs();
//    dump_everything();
//
//    RelaxedState rs = RelaxedState(*g_initial_state);
//    buildTestState(*g_initial_state);
//    cout << "test state:" << endl;
//    g_initial_state->dump();
//    rs = RelaxedState(*g_initial_state);

    BestFirstSearchEngine *engine = new BestFirstSearchEngine;
    if(cg_heuristic || cg_preferred_operators)
	engine->add_heuristic(new CGHeuristic, cg_heuristic,
		cg_preferred_operators);
    if(cyclic_cg_heuristic || cyclic_cg_preferred_operators)
	engine->add_heuristic(new CyclicCGHeuristic, cyclic_cg_heuristic,
		cyclic_cg_preferred_operators);
    if(no_heuristic)
	engine->add_heuristic(new NoHeuristic, no_heuristic, false);

    
    double best_makespan = REALLYBIG;
    
    int plan_number = 1;
    while(true) {
	times(&search_start);
	engine->initialize();
	engine->search();
	times(&search_end);
	if(engine->found_solution()) {
	    best_makespan = save_plan(engine->get_plan(),best_makespan,plan_number,plan_name);
	    engine->fetch_next_state();
	}
	else {
	    break;
	}
    }
    engine->statistics();
    
    
    if(cg_heuristic || cg_preferred_operators) {
	cout << "Cache hits: " << g_cache_hits << endl;
	cout << "Cache misses: " << g_cache_misses << endl;
    }
    int search_ms = (search_end.tms_utime - search_start.tms_utime) * 10;
    cout << "Search time: " << search_ms / 1000.0 << " seconds" << endl;
    int total_ms = (search_end.tms_utime - start.tms_utime) * 10;
    cout << "Total time: " << total_ms / 1000.0 << " seconds" << endl;

    return engine->found_at_least_one_solution() ? 0 : 1;
}

double save_plan(const vector<PlanStep> &plan, double best_makespan, int &plan_number, string &plan_name) {
    
    double makespan = 0;
    for(int i = 0; i < plan.size(); i++) {
	double end_time = plan[i].start_time + plan[i].duration;
	makespan = max(makespan,end_time);
    }
    
    cout << "Makespan = " << makespan << endl;
    
    if(makespan >= best_makespan) return best_makespan;
    
    int len = static_cast<int>(plan_name.size() + log10(plan_number) + 10);
    char *temp_filename = (char*)malloc(len * sizeof(char));
    sprintf(temp_filename, "%s.%d.in", plan_name.c_str(), plan_number);
    char *filename = (char*)malloc(len * sizeof(char));
    sprintf(filename, "%s.%d", plan_name.c_str(), plan_number);
    len = 2*len + 30;
    char *syscall = (char*)malloc(len * sizeof(char));
    sprintf(syscall, "./search/epsilonize_plan.py < %s > %s", temp_filename, filename);
    char *syscall2 = (char*)malloc(len * sizeof(char));
    sprintf(syscall2, "rm %s", temp_filename);
      
//    printf("FILENAME: %s\n", temp_filename);
    FILE *file = fopen(temp_filename,"w");
 
    plan_number++;
    for(int i = 0; i < plan.size(); i++) {
	const PlanStep& step = plan[i];
	fprintf(file,"%.3f: (%s) [%.3f]\n", step.start_time, step.op->get_name().c_str(), step.duration);
	printf("%.3f: (%s) [%.3f]\n", step.start_time, step.op->get_name().c_str(), step.duration);
    }
    fclose(file);

    cout << "Plan length: " << plan.size() << " step(s)." << endl;
    cout << "Makespan   : " << makespan << endl;

    if(system(syscall) != 0) {
      // silence gcc warning
    }
    if(system(syscall2) != 0) {
      // silence gcc warning
    }

    free(temp_filename);
    free(filename);
    free(syscall);
    free(syscall2);
    
    return makespan;
    
}
