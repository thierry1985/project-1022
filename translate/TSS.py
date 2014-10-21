#! /usr/bin/env python

import sys, os, time
import axiom_rules, fact_groups, instantiate, pddl
import sas_tasks, simplify
from translate import *
import TemporalSaseEncoding


ALLOW_CONFLICTING_EFFECTS = False
USE_PARTIAL_ENCODING = True
WRITE_ALL_MUTEXES = True

##############  End of Debug Use classes and functions ###############


#Make SAS task in the simplest form that we need;
#Must have: transitions, operators, goal, init
class TemporalTask():
    def __init__(self,sas):
        self.ranges = sas.variables.ranges   #list of integers;
        if self.ranges.count(-1)!=0:
            i = self.ranges.index(-1)
            self.ranges = self.ranges[0:i] 
        self.transitions = []
        self.trans_dict  = {}
        self.opers = []
        self.goal  = []
        self.init  = []
        
        #Auxilary variables;
        self.ft_names = {}
        
        #Record ft names
        vars = {}
        for exp,[(var, val)] in sas.strips_to_sas.iteritems():
            vars.setdefault(var, []).append((val, exp))
        for var in range(len(vars)):
            if var >= len(self.ranges): continue
            vals = sorted(vars[var]) 
            for (val, exp) in vals:  self.ft_names[(var,val)] = exp
        
        #Init and Goals;
        for mv,val in enumerate(sas.init.values): self.init.append((mv,val))
        for mv,val in sas.goal.pairs: self.goal.append((mv,val))
        
        #Build up Clean Transitions & Actions:
        assert len(sas.operators) == 0
        for op in sas.temp_operators:
            if op.__class__.__name__ == "TransBasedOperator": 
                op.duration = int(op.duration)
                self.opers.append(op)
                continue
            newOp = TransBasedOperator()
            newOp.name = op.name
            newOp.duration = int(op.simple_duration)
            assert newOp.duration != 0
            for time in range(3):
                for var, val in op.prevail[time]:
                    if time == 0:   newOp.start.add( (var,val,val))
                    elif time == 1: newOp.overall.add( (var,val,val) )
                    elif time == 2: newOp.end.add( (var,val,val) )
                    else:           assert False
            for time in range(2):
                for var,pre,post,cond in op.pre_post[time]:
                    if time == 0:   newOp.start.add( (var,pre,post) )
                    elif time == 1: newOp.end.add( (var,pre,post) )
                    else:           assert False
            self.opers.append(newOp)
                  
            
        for i,op in enumerate(self.opers):
            self.add_to_transdb( i, op.start )
            self.add_to_transdb( i, op.overall )
            self.add_to_transdb( i, op.end )
        
        
        #Prevailing transitions, makes self.transitions larger
        for mv,r in enumerate(self.ranges):
            for i in range(r):
                if (mv,i,i) in self.trans_dict: continue
                self.trans_dict[mv,i,i] = len(self.transitions)
                self.transitions.append((mv,i,i))
        
        #Beta. Transitions list is done, make a link back from tran to oper
        self.prepost_conn_start = [[] for i in self.transitions]
        self.prepost_conn_end   = [[] for i in self.transitions]
                
        #The transition set for each individual multi-var
        self.mv_trans = [[] for i in self.init]
        for mv,r in enumerate(self.ranges):
            for i in range(r):
                for j in range(r):
                    if (mv,i,j) in self.trans_dict:
                        index = self.trans_dict[mv,i,j]
                        self.mv_trans[mv].append(index)
        
        #Make the action representation more compact
        for oper in self.opers:
            oper.start = [self.trans_dict[item] for item in oper.start]
            oper.overall = [self.trans_dict[item] for item in oper.overall]
            oper.end = [self.trans_dict[item] for item in oper.end]            
        
        
        #Beta. Really doing things
        for op_i,oper in enumerate(self.opers):
            for tr_i in oper.start:
                assert type(tr_i) == type(1)
                self.prepost_conn_start[tr_i].append(op_i)
            for tr_i in oper.end: 
                assert type(tr_i) == type(1)
                self.prepost_conn_end[tr_i].append(op_i)        
            
            for r in sas.variables.ranges:
                assert r!=0
                if r >0: self.ranges.append(r)
        
        #Something wrong with the parser, need to hack this action;
        for op_i,oper in enumerate(self.opers):
          if "serve" in oper.name:
            rem = self.transitions[oper.start[0]][0]
            for tr_i in oper.overall:
              if self.transitions[tr_i][0]!=rem:
                oper.start.append(tr_i)
                
    def add_to_transdb(self,i,trans_lst):
        for t in trans_lst:
            assert len(t) == 3            
            if t not in self.trans_dict:
                self.trans_dict[t] = len(self.transitions)
                self.transitions.append(t)
            
            
    def get_ftname(self,mvar):
        return self.ft_name[mvar]
    
    def output(self):
        print self.ranges        
        print self.init
        print self.goal
        for k in self.ft_names.keys():  print self.ft_names[k]

#######################################################################
#######################################################################
#######################################################################
#######################################################################
#######################################################################
#######################################################################

def print_out_task(task):
  out = sys.stdout
  ACTION_NAME_ONLY = False
  
  print "Objects"
  for object in task.objects:
    out.write( object.__str__() )
    out.write( ";  ")
  
  print "\n Facts/Atoms"
  for fact in task.facts:
    out.write( fact.__str__().split(" ")[1] )
    out.write(";  ")
  
  print "\n Initial State"
  
  init = []
  for fact in task.init:
    if fact in task.facts:
      init.append( fact.__str__() )
      out.write( fact.__str__() )
      out.write("; ")
    
  print "\n Goals \n"
  for fact in task.goal.parts:
    out.write( fact.__str__().split(" ")[1] )
    out.write("; ")

  
  print "\n Grounded Operators"
  for oper in task.opers:
    if ACTION_NAME_ONLY:
      print oper.line_str()
    else:
      oper.dump()

  print "\n End of Task Print Out"


def print_soln_plan(task,plan):
  if len(sys.argv) == 4:
    fwrite = file(sys.argv[3],"w")
    import resource
    print >> fwrite, ";Span ", len(plan)
    print >> fwrite, ";Time",resource.getrusage(resource.RUSAGE_SELF)[0]
    print >> fwrite, ""
    for t in range(0,len(plan)):
      for op_index in plan[t]:
        print >>fwrite, "%d:%s [%d] [%d]" % (t, task.opers[op_index].name, \
                  int(task.opers[op_index].duration), int(task.opers[op_index].cost) )
    fwrite.close()
  else:
    print "Time Span:", len(plan)
    for t in range(0,len(plan)):
      for op_index in plan[t]:
        print "%d:%s [%d]" % (t, task.opers[op_index].name, \
                int(task.opers[op_index].duration))


if __name__ == "__main__":
    
    import pddl
    print "Parsing..."
    task = pddl.open()

    sas_task = pddl_to_sas(task)
    del task
    
    task = TemporalTask(sas_task)
    
    make_span = 10
    econ = TemporalSaseEncoding.TemporalSaseEncoding(task)
    SOLVE_BOUND = 200
    while make_span < SOLVE_BOUND:
        plan = econ.solve_decision(task,make_span)
        if len(plan)!=0:
            print "Found a plan"
            print_soln_plan(task,plan)
            break
        make_span += 1
        #break
    if make_span == SOLVE_BOUND:
        print "Fail to solve in ", SOLVE_BOUND, " steps."
    #print_out_task(sas_task)
    
    #print "Writing output..."
    #sas_task.output(file("output.sas", "w"))
    #print "Done!"

