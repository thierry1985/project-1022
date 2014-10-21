#! /usr/bin/env python2.5
# -*- coding: latin-1 -*-

import axiom_rules
import fact_groups
import instantiate
import pddl
import sas_tasks
import simplify
import sys
import PrecosatSolver
import copy
import os
import SASE_helper
import SolverAdapter
from SASE_helper import *
from SASE_base import *
import math
import signal


#DEBUGGING OPTIONS
PRINT_OUT_CLAUSES = False
PRINT_ALL_OP_AX   = False
USE_CLIQUE_BIN_CONSTRAINT = True


def signal_handler(signum, frame):
  raise Exception("Time out.")

################################################
## This basic encoding doesn't support axiom (i.e. STRIPS only)
## Two Types of vars: trans, op
################################################
class PlainSASEncoding(BaseSASEncoding):

  def print_all_variables(self):
    for var in range(self.var_cnt-1):
      t = self.var_lnk[var] 
      if t[0] == "trans":
        if len(t) == 5:
          print var+1, "is var[%d:%d->%d@%d]" % (t[1],t[2],t[3],t[4])
      elif t[0] ==  "op":
        print var+1, "is op[%d:%s@%d]" %(t[1], self.task.operators[t[1]].name, t[2])
      elif t[0] == "AUX":
        print var+1, "is AUX for %dth bit of var set:" % ( t[1] ), t[2]
      else:
        pass

  def init_encoding(self):
    self.op_var     = {}   #Each key in the dictionary is a 2-tuple (op_index, time)
    self.trans_var  = {}
    self.var_lnk = []
    self.var_cnt = 1
    


  def encoding_specific_processing(self):
    self.useful_trans_cnt = len(self.trans_lst)
    for mv in range(len(self.mv_ranges)):
      for v in range(self.mv_ranges[mv]):
        if (mv,v,v) not in self.trans_dict:
          self.trans_dict[mv,v,v] = len(self.trans_lst)
          self.trans_lst.append( (mv,v,v))
          self.pres[mv][v].add(v)
          self.posts[mv][v].add(v)
    


  def make_op_var( self, op, time ):
    assert type(op) == type(1)
    assert (op,time) not in self.op_var
    
    self.op_var[op,time] = self.var_cnt
    self.var_lnk.append( ("op", op, time)  )
    self.var_cnt += 1
    return self.var_cnt - 1


  def make_trans_var(self, mv, pre, post, time):
    assert (mv,pre,post,time) not in self.trans_var
    self.trans_var[mv,pre,post,time] = self.var_cnt
    self.var_lnk.append( ("trans", mv, pre, post, time)  )
    self.var_cnt += 1
    return self.var_cnt - 1
  
  
  #############################################################################
  def add_clique_exclusion_constraints(self,s,lits):
    if len(lits)<4:
      for i,lit in enumerate(lits):
        for lit2 in lits[i+1:]:
          s.new_clause_add_lit( -1 * lit )
          s.new_clause_add_lit( -1 * lit2 )
          s.new_clause_push()
    else:
      num_aux_vars =  int( math.ceil( math.log(len(lits),2) ) )
      aux_vars = []
      for i in range(num_aux_vars):
        aux_vars.append(self.var_cnt)         
        self.var_lnk.append(("AUX",i,set(lits)))
        self.var_cnt += 1
        
      for order,lit in enumerate(lits):
        binstr = dec2bin(order)
        assert len(binstr) <= num_aux_vars        
        binstr = "0" * (num_aux_vars - len(binstr) ) + binstr 
        for i,bit in enumerate(binstr):
          s.new_clause_add_lit( -1 * lit )
          if bit == '0':
            s.new_clause_add_lit( -1 * aux_vars[i])
          else:
            s.new_clause_add_lit( aux_vars[i]) 
          s.new_clause_push()
          
        # This type of additional clause will rule out all the solutions, why???????????????
        #Other Direction;
        #s.new_clause_add_lit( lit )
        #for i,bit in enumerate( binstr ):
        #  if bit == '0':
        #    s.new_clause_add_lit( aux_vars[i])
        #  else:
        #    s.new_clause_add_lit( -1 * aux_vars[i] )
        #s.new_clause_push()
        
      #""" Do WE NEED THIS!!!!????
      #for order in range(len(lits),2**num_aux_vars):
      #  binstr = dec2bin(order)
      #  binstr = "0" * (num_aux_vars - len(binstr) ) + binstr
      #  for i,bit in enumerate(binstr):
      #    if bit == '0':
      #      s.new_clause_add_lit( aux_vars[i])
      #    else:
      #      s.new_clause_add_lit( -1 * aux_vars[i])
      #  s.new_clause_push()
      
  def add_trans_mutual_exclusions(self,s,mv,time):
    if USE_CLIQUE_BIN_CONSTRAINT:
      clique = set()
      for pre in range(0,self.mv_ranges[mv]):
        for post in range(0,self.mv_ranges[mv]):
          if ( mv, pre, post, time ) in self.trans_var:
            var = self.trans_var[ mv, pre, post, time ]
            clique.add(var)
      for val in range(self.mv_ranges[mv]):
        if (mv,val,val,time) in self.trans_var:
          var = self.trans_var[mv,val,val,time]
          clique.add(var)
      self.add_clique_exclusion_constraints( s, list(clique) )
      
      #Special Case for V:-1->X:
      #to-do: This condition might be flawed.  
      #Are we sure that -1->X and Y->X should be excluded or not?
      for val in range(self.mv_ranges[mv]):
        if (mv,-1,val,time) in self.trans_var:
          tvar = self.trans_var[mv,-1,val,time]
          ex_var  = -1
          if (mv,val,val,time) in self.trans_var:
            ex_var = self.trans_var[ mv,val,val,time ]
          for lit in list(clique):
            if ex_var != lit:
              s.new_clause_add_lit( -1 * tvar )
              s.new_clause_add_lit( -1 * lit )
              s.new_clause_push()
      
      #Another possible choice:
      #for val in range(self.mv_ranges[mv]):
      #  if (mv,-1,val,time) in self.trans_var:
      #    tvar = self.trans_var[mv,-1,val,time]
      #    excluded_var_set = set(clique)
      #    for pre in range(0,self.mv_ranges[mv]):
      #      if (mv,pre,val,time) in self.trans_var:
      #        excluded_var_set.remove( self.trans_var[mv,pre,val,time] )
      #    for lit in list(excluded_var_set):
      #        s.new_clause_add_lit( -1 * tvar )
      #        s.new_clause_add_lit( -1 * lit )
      #        s.new_clause_push()      
      
    else:
      for pre in range(-1,self.mv_ranges[mv]):
        for post in range(0,self.mv_ranges[mv]):
          if ( mv, pre, post, time ) in self.trans_var:
            var = self.trans_var[ mv, pre, post, time ]
            
            for val in range(self.mv_ranges[mv]):
              if (val,val) != ( pre,post ) and (mv, val, val, time ) in self.trans_var:
                var2 = self.trans_var[ mv, val, val, time ]
                s.new_clause_add_lit( -1 * var )
                s.new_clause_add_lit( -1 * var2 )
                s.new_clause_push() 
                
            for others in self.mv_trans[mv]:
              assert len(others) == 2
              if others[0]==others[1] or others == ( pre,post ):
                continue
              if  (mv, others[0], others[1], time ) in self.trans_var:
                var2 = self.trans_var[ mv, others[0], others[1], time ]
                s.new_clause_add_lit( -1 * var )
                s.new_clause_add_lit( -1 * var2 )
                s.new_clause_push()
  
  
  ############################################################################
  ####                   Encoding of each iteration;                      ####
  ############################################################################
  def solve_decision( self, task,  N ):
    
    self.init_encoding()
    std = sys.stdout
    #s = minisat.Solver()
    s = PrecosatSolver.Solver()
    
    if self.status.dump != 0 :
      #assert False
      global PRINT_OUT_CLAUSES
      PRINT_OUT_CLAUSES = True
      s = SolverAdapter.Solver()
    
    ################  Constructing Variables    ################
    for trans,index in self.trans_dict.items():
      for time in range(self.first_apr_trans[index],N):
        if N - time < self.dist_to_N[index]:
          continue
        self.make_trans_var(trans[0], trans[1], trans[2], time )
    print "Total Number of Trans Variables %d;" % ( self.var_cnt ), 
    
    reduced_cnt = 0
    for time in range(N):
      flag = False
      for op_i,op in enumerate(self.task.operators):
        if self.first_appearance[op_i] > time:
          continue
        flag = False
        for mv,pre,post,cond in op.pre_post:
          if N - time < self.dist_to_N[self.trans_dict[mv,pre,post]]:
            flag = True
            break
        if not flag:
          if op_i not in self.reduced_op_dict:
            self.make_op_var( op_i, time)
          else:
            t = self.reduced_op_dict[op_i]
            if len(t) == 1:
              tran = self.trans_lst[t[0]]
              chk_v = (tran[0],tran[1],tran[2],time)
              if chk_v in self.trans_var:
                reduced_cnt += 1
                self.op_var[op_i,time] = self.trans_var[chk_v]
            else:
              self.make_op_var( op_i, time)
    print " Op Variables ", self.var_cnt, 'reduced by %d' %( reduced_cnt )
    
    #################    Part I.  Constraints Of Transitions  ##################### 
    #Constraint Type 1: Exclusiveness of Underlying Atomic Transitions
    for time in range(N):
      for mv in [mv for mv in range(len(self.mv_ranges))]:
        self.add_trans_mutual_exclusions(s,mv,time)
    print_clause_progress("I.a  (Trans Mutex)",s)
    
    
    #Constraint 2: Progressive relations of underlying Atomic transitions
    #            : value at N implies a disjunction of corresponding value changes at N+1
    #            : Axiom MVs are not included
    #############  This is forwarding approach
    for time in range( 0, N-1 ):
      for mv in range(self.num_mv):
        for pre in range(-1,self.mv_ranges[mv]):
          for post in range(self.mv_ranges[mv]):
              
            if ( mv, pre, post, time) not in self.trans_var:
              continue
              
            clause = []
            var = self.trans_var[ mv, pre, post, time ]
            clause.append( var * -1)
            
            for i in range(0, self.mv_ranges[mv]):
              if ( mv, post, i, time + 1) in self.trans_var:
                var2 = self.trans_var[ mv, post, i, time + 1]
                clause.append( var2 )
            
              if (mv,-1,i,time+1) in self.trans_var:
                var2 = self.trans_var[ mv, -1, i, time + 1]
                clause.append( var2 )
            
            if len(clause)!=1:
              for lit in clause:
                s.new_clause_add_lit(lit)
              s.new_clause_push()
    print_clause_progress("I.b Transitions's progression", s)
    
    # Constraint 2: in a backward way;
    for time in range( 1, N ):
      for mv in range(self.num_mv):
        for pre in range(0,self.mv_ranges[mv]):
          for post in range(self.mv_ranges[mv]):
            if ( mv, pre, post, time) not in self.trans_var:
              continue
              
            clause = []
            var = self.trans_var[ mv, pre, post, time ]
            clause.append( var * -1)
            
            for i in range(0, self.mv_ranges[mv]):
              if ( mv, i, pre, time - 1) in self.trans_var:
                var2 = self.trans_var[ mv, i, pre, time - 1]
                clause.append( var2 )

              if (mv,-1,pre,time-1) in self.trans_var:
                var2 = self.trans_var[ mv, -1, pre, time - 1]
                clause.append( var2 )
            
            #if len(clause)!=1:  I guess this is not necessary;
            for lit in clause:
              s.new_clause_add_lit(lit)
            s.new_clause_push()
    print_clause_progress("I.b Transitions's progression (Backward)", s)
    
    #Constraint 6: Initial state
    for mv,val in enumerate(task.init.values):
      for possible_post in range(0,self.mv_ranges[mv]):
        if ( mv, val, possible_post, 0 ) in self.trans_var:
          var = self.trans_var[ mv, val, possible_post, 0 ]
          s.new_clause_add_lit( var )
      for vv in range(self.mv_ranges[mv]):
        if(mv,-1,vv,0) in self.trans_var:
          s.new_clause_add_lit( self.trans_var[mv,-1,vv,0] )
      s.new_clause_push()
    print_clause_progress("I.c  Inital state", s )
    
    
    #Constraint 7: Goal:
    for mv,val in task.goal.pairs:
      clause = []
      debug_lst = []
      for possible_pre in range(-1,self.mv_ranges[mv]):
        if (mv, possible_pre, val, N-1) in self.trans_var:
          var = self.trans_var[ mv, possible_pre, val, N-1 ]
          clause.append( var )
        else:
          debug_lst.append((mv, possible_pre, val, N-1))
      if len(clause)==0:
        del s
        return []
      else:
        for lit in clause:
          s.new_clause_add_lit(lit)
      s.new_clause_push()
    print_clause_progress("I.d  Goal",s)
    
    
    #####################################################################
    ############ #    Part II.  Constraints with Op involved ###########     
    #####################################################################
    
    #Constraint Type 0: At least one action needs to be true at each time step; 
    #This type of constraints is just not necessary here;
    #for time in range(N):
    #  flag = False
    #  for op_i,op in enumerate(task.operators):
    #    if self.first_appearance[op_i] > time:
    #      continue
    #    s.new_clause_add_lit( self.op_var[ op_i, time] )
    #    flag = True
    #  if flag:
    #    s.new_clause_push()
    #print_clause_progress("II.a  (Action's Existence):", s)

    #Constraint 4: Mapping from trans to actions (one transition implies a disj of actions)
    for index in range(len(self.trans_conn)):
      trans = self.trans_lst[index]
      if trans[1] == trans[2]:
        continue
      if index > self.useful_trans_cnt:
        continue
        
      for time in range( self.first_apr_trans[index], N - self.dist_to_N[index] ):
        lits = []
        
        trans_var = self.trans_var[ trans[0], trans[1], trans[2], time ]
        lits.append( trans_var * -1 )
        for op in list(self.trans_conn[index]):
          if (op,time) in self.op_var:
            opv = self.op_var[op,time]
            lits.append( opv )
        
        #Remember, This condition check is necessary!!
        #trans[1]!=trans[2]: make sure those non-change transition
        #   even they don't imply anything, will not be pruned.
        if trans[1]!=trans[2] or len(lits)>1:
          for lit in lits:
            s.new_clause_add_lit( lit )
          s.new_clause_push()
    print_clause_progress("II.b  Trans => Opers", s)

    #Constraint 3: Mapping from actions to transitions (one action implies a conjunction of transitions)
    # Any trans_var doesn't exists, there must be an exception!!!
    for op_i,op in enumerate(task.operators):
      for time in range(N):
        if (op_i,time) not in self.op_var:
          continue
        op_var = self.op_var[ op_i, time ]
        
        for mv, pre, post, cond in op.pre_post:
          assert len(cond) == 0
          
          chk_var = ( mv, pre, post, time )
          assert chk_var in self.trans_var
          trans_var = self.trans_var[chk_var]
          
          s.new_clause_add_lit( -1 * op_var )
          s.new_clause_add_lit( trans_var )
          s.new_clause_push()
            
        for mv, val in op.prevail:
          chk_var = ( mv, val, val, time )
          assert chk_var in self.trans_var
          trans_var = self.trans_var[chk_var]
          
          s.new_clause_add_lit( -1 * op_var )
          s.new_clause_add_lit( trans_var )
          s.new_clause_push()
    print_clause_progress("II.c  Opers => Trans",s)

    #Constraint 4: Mutex Conditions; 
    #Two actions with same pre-post transition cannot be executed at the same time; (a violation)
    if USE_CLIQUE_BIN_CONSTRAINT :
      for time in range(0,N):
        for trans,index in self.trans_dict.items():
          if trans[1]==trans[2]:# or trans[1] == -1:
            continue
          
          if index in self.unnecessary_cliques: #and time > self.clique_first_required_time[index]:
            continue

          if len(self.trans_conn[index]) > 1:
            clique = []
            for op1 in self.trans_conn[index]:
              if (op1,time) not in self.op_var:
                continue
              op_var = self.op_var[op1,time]
              clique.append( op_var )
            
            #if index in self.unnecessary_cliques and len(clique)==len(self.trans_conn[index]):
            #  continue

            if len(clique)>1:
              self.add_clique_exclusion_constraints(s,clique)
    else:
      for trans,index in self.trans_dict.items():
        if trans[1]==trans[2]:
          continue
        
        if len(self.trans_conn[index]) > 1:
          lst = list(self.trans_conn[index])
          for i,op1 in enumerate(lst):
            for op2  in lst[i+1:]:
              if op1 == op2:
                continue
              for time in range(0,N):
                if ((op1,time) not in self.op_var) or ( (op2,time) not in self.op_var):
                  continue
                op1_var = self.op_var[ op1, time ]
                op2_var = self.op_var[ op2, time ]
                s.new_clause_add_lit( -1 * op1_var )
                s.new_clause_add_lit( -1 * op2_var )
                s.new_clause_push()
    print_clause_progress("II.d  Opers' mutex",s)
    print "SASE Encoding finished. #Var:", self.var_cnt 
    
    
    #Conflict Effects! This constraint only applies to Rovers domain
    #This is a hack!!
    for time in range(0,N):
      for item in self.conflict_eff_cliques:
        clique = []
        for op in item:
          if (op,time) in self.op_var:
            clique.append(  self.op_var[op,time] )
        self.add_clique_exclusion_constraints(s,clique)
    
    
    ######1. Implication Trans;
    for index,implied_trans in self.trans_implication.items():
      mt = self.trans_lst[index]
      for time in range(N):
        if(mt[0],mt[1],mt[2],time) in self.trans_var:
          var = self.trans_var[mt[0],mt[1],mt[2],time]

          for i2 in list(implied_trans):
            it = self.trans_lst[i2]
            mv,val,val2 = it
            for pre in range(-1,self.mv_ranges[mv]):
              if pre!= val and (mv,pre,val,time) in self.trans_var:
                s.new_clause_add_lit( -1 * var)
                var2 = self.trans_var[mv,pre,val,time]
                s.new_clause_add_lit( -1 * var2)
                s.new_clause_push() 
            #for post in range(-1,self.mv_ranges[mv]):
            #  if post!=val2 and (mv,val2,post,time) in self.trans_var:
            #    s.new_clause_add_lit( -1 * var)
            #    var2 = self.trans_var[mv,val2,post,time]
            #    s.new_clause_add_lit( -1 * var2)
            #    s.new_clause_push() 
    print_clause_progress("III.b Implied Transitions ", s)
    self.encoding_post_operations( self.var_cnt, s )


    if PRINT_OUT_CLAUSES == True:
      if self.status.output == "":
        self.print_all_variables()
        s.print_out_all_clauses()
        s.solver()
      else:
        s.dump_all_clauses( self.status.output )
      sys.exit()
    else:
      signal.signal(signal.SIGALRM, signal_handler)
      signal.alarm(self.status.time_limit)
      rst = s.solve()
      
    if rst == False:
      del s
      return []
    else:
      plan = self.decode( s, task, N )
      del s
      return plan


  def decode(self, solver, task, N ):
    print "Decoding for total time layer ", N
    
    plan = [[] for item in range(N)]
    
    for t in range( 0, N ):
      for index,o_oper in enumerate(task.operators):

          if (index,t) in self.op_var:
            var = self.op_var[index, t]
            #rst = solver.model.element(var-1).get_v()
            rst = solver.get_assignment(var)
            if rst == 1:
              if self.first_appearance[index] > t:
                print "Checking and Adding Action:", o_oper.name
                print "However, graph-plan info says it can only be true starting from layer", self.first_appearance[index]
              plan[t].append(index)
              
    for item in plan:
      print item,
    return plan

### END OF ENCODING_CONSTANT class ###################################################
