#! /usr/bin/env python2.5
# -*- coding: latin-1 -*-
import os, sys
import axiom_rules
import fact_groups
import instantiate
import pddl, simplify
import SATSolver
import math

TYPE_TRAN_CONST = 1
TYPE_OPER_CONST = 2

def dec2bin(i):
  b = ''
  while i > 0:
    j = i & 1
    b = str(j) + b
    i >>= 1
  return b
  
def transition_to_str(t):
    return str(t[0])+":"+ str(t[1]) + "->" + str(t[2])
    
class TemporalSaseEncoding(object):
  
  def __init__(self,task):
    self.task = task
    self.opers = task.opers
    self.transitions = task.transitions
    self.trans_dict = task.trans_dict
    self.mv_trans = task.mv_trans
    self.prepost_conn_start = task.prepost_conn_start
    self.prepost_conn_end  = task.prepost_conn_end
    self.ranges = [item  for item in task.ranges[0:len(task.init)] ]
    m = 0
    for tr in self.transitions:  m = max(m,tr[0])
    self.ranges = self.ranges[0:m+1]
    
    
    #New data structures
    self.mv_trans = task.mv_trans
    print "Encoding Constant Initilized!"
    
    self.tr_first_appr = [None for i in self.transitions]
    self.op_first_appr = [0 for i in self.opers]
    cumu_var_set       = [set() for i in range(len(self.ranges))]
    
    for i,r in enumerate(self.ranges):
      init = self.task.init[i]
      cumu_var_set[init[0]].add(init[1])
      cumu_var_set[init[0]].add(-1)
      index = self.trans_dict[init[0],init[1],init[1]]
      self.tr_first_appr[index] = 0
      
    
    for t in range(200):
      flag = False
      for mv in range(len(self.ranges)):
        for old_val in list(cumu_var_set[mv]):
          for index in self.mv_trans[mv]:
            if self.tr_first_appr[index]!=None: continue
            tr = self.transitions[index]
            #print tr, old_val
            if (tr[1] == old_val or tr[1]!=-1) and (tr[2] not in cumu_var_set[mv]):
              cumu_var_set[mv].add(tr[2])
              self.tr_first_appr[index] = t
              i = self.trans_dict[mv,tr[1],tr[1]]
              if self.tr_first_appr[i]!=None: continue
              self.tr_first_appr[i] = t+1
              flag = True
      if not flag: break

    
  def add_clique_constraints(self,s,lits):
    if len(lits)<=1: return
    
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
        self.var_cnt += 1        
        aux_vars.append(self.var_cnt)
        self.var_lnk.append(("AUX",i,set(lits)))
      
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
      

      for order in range(len(lits),2**num_aux_vars):
        binstr = dec2bin(order)
        binstr = "0" * (num_aux_vars - len(binstr) ) + binstr
        for i,bit in enumerate(binstr):
          if bit == '0':  s.new_clause_add_lit( aux_vars[i])
          else:           s.new_clause_add_lit( -1 * aux_vars[i])
        s.new_clause_push()

  #for debug
  def print_all_variables(self):
    #Print (mv,val) ==> name;
    out = open("debug.vars","w")
    print >> out, "All MVs"
    for mv,r in enumerate(self.ranges):
        print >> out, mv, ":", 
        for i in range(r):
            print >> out, i, "--", self.task.ft_names.get((mv,i)), ";",
        print >> out, ""
    #Print transitions
    print >> out, "All transitions:"
    for i,tr in enumerate(self.transitions):
        print >> out, i,":",tr
    #Print Oper.name \n  ([],[],[])
    print >> out, "All opers:" 
    for index,oper in enumerate(self.opers):
        print >>out, oper.name,
        print >>out, "Start",oper.start, 
        print >>out, "Overall",oper.overall,
        print >>out, "End",oper.end
        #oper.output(out)
        
    #print (mv,pre,post) t ==> VAR
    print >> out, "TR_VAR#",len(self.tran_var.keys())
    for i,tr in enumerate(self.transitions):
        for t in range(self.N):
            if (i,t) in self.tran_var:
                print >> out, tr[0], ":", tr[1], "->", tr[2], "@", t, "<%d>" %( self.tran_var[i,t] )
    
    print >> out, "OP_VAR#",len(self.oper_var.keys())
    for i,op in enumerate(self.opers):
        for t in range(self.N):
            if (i,t) in self.oper_var:
                print >> out, op.name, t, ">>", self.oper_var[i,t]
    

  #This N means N layers of actions!!!!!!!!!!!!!!!!!!
  def solve_decision( self, task,  N):
    
    self.N = N
    s = SATSolver.Solver()
    self.tran_var = {}  #Each key in the dictionary is a 2-tuple (trans index, time )
    self.oper_var = {}  #Each key in the dictionary is a 2-tuple (op index, time)
    self.var_lnk = []  #mapping var index ==> either (t,trans) or  (f,oper)
    self.var_cnt = 0
    
    #Build up all variables
    """
    for t in range(N):
      for i in range(len(self.transitions)):
        self.var_cnt += 1
        self.tran_var[i,t] = self.var_cnt
        self.var_lnk.append((TYPE_TRAN_CONST, i, t))
    for t in range(N):
      for i in range(len(self.opers)):
        assert type(self.opers[i].duration) == type(1)
        
        if t + self.opers[i].duration - 1 > N - 1: continue
        self.var_cnt += 1
        self.oper_var[i,t] = self.var_cnt
        self.var_lnk.append((TYPE_OPER_CONST, i, t))
    """

    tran_candidate = set()
    oper_candidate = set()
    for t in range(N):
      for i in range(len(self.transitions)):
        if self.tr_first_appr[i] > t : continue
        tran_candidate.add( (i,t) )
      for i in range(len(self.opers)):
        assert type(self.opers[i].duration) == type(1)
        if t + self.opers[i].duration - 1 > N -1 : continue
        oper_candidate.add( (i,t) )
    #"""
    saved_cnt = 0
    for index,tr in enumerate( self.transitions ):
      if tr[1] == tr[2]: continue
      for t in range(N):
        flag = False
        for op in self.prepost_conn_start[index]:
          if (op,t) in oper_candidate:
            flag = True
            break
        if flag: continue
        
        for op in self.prepost_conn_end[index]:
          du = self.opers[op].duration
          if (op,t-du+1) in oper_candidate: 
            flag = True
            break
        if not flag:  
          tran_candidate.remove( (index,t) )
          saved_cnt += 1
    print "Phase A Saved cnt", saved_cnt
    for op_i,oper in enumerate(self.task.opers):
      for t in range(0,N):
        if (op_i,t) not in oper_candidate: continue
        for tr_i in oper.overall:
            for t2 in range( t+1, t+oper.duration-1 ):
                if (tr_i,t2) not in tran_candidate:
                    oper_candidate.remove( (oper_i,t) )
                    saved_cnt += 1
                    break
        for tr_i in oper.start:
            if (tr_i,t) not in tran_candidate:
              oper_candidate.remove( (oper_i,t) )
              saved_cnt += 1
              break
    for i,e in enumerate(self.tr_first_appr):
      if e == None: self.tr_first_appr[i] = 0
      if self.transitions[i][1] == -1: self.tr_first_appr[i] = 0
    
    
    print "Phase B Saved cnt", saved_cnt
    #"""
    for (i,t) in list(tran_candidate):
        self.var_cnt += 1
        self.tran_var[i,t] = self.var_cnt
        self.var_lnk.append((TYPE_TRAN_CONST, i, t))
    for (i,t) in list(oper_candidate):
        self.var_cnt += 1
        self.oper_var[i,t] = self.var_cnt
        self.var_lnk.append((TYPE_OPER_CONST, i, t))
    
    #print self.trans_dict[1,1,1]
        
    """
          #1. initial state and goal state
    """
    print "Solving with make span:", N
    print "Encoding init,...                      ", s.nClauses()
    for i,r in enumerate(self.ranges):
      init = self.task.init[i]
      lst = []
      for possible_post in range(self.task.ranges[i]):
          tran = (init[0],init[1],possible_post)
          if tran in self.task.trans_dict:
              index = self.task.trans_dict[tran]
              var = self.tran_var.get( (index, 0) )
              lst.append( var )
      for index in self.mv_trans[i]:
          if self.transitions[index][1] == -1:
              var = self.tran_var.get( (index,0) )
              lst.append(var)

      if len(lst)>0: 
        for var in lst:
          if var: s.new_clause_add_lit( var )
        s.new_clause_push()
      else:
        return []
    
    print "Encoding goal...                       ", s.nClauses()
    for p in task.goal:
      mv,val = p
      lst = []
      for possible_pre in range(-1,self.task.ranges[mv]):
        tran = (mv,possible_pre,val)
        index = self.trans_dict.get( tran )
        if index:
          index = self.trans_dict[tran]
          var = self.tran_var.get( (index, N-1)  )
          lst.append(var)
      if len(lst)>0:
        for var in lst: 
          if var:  s.new_clause_add_lit( var ) 
        s.new_clause_push()
          

    """
          Transition at time t ==> Actions t+du PR actions t-du
    """
    print "Encoding transitions => actions...     ", s.nClauses()
    for index,tr in enumerate( self.transitions ):
      if tr[1] == tr[2]: continue
      #if tr[1] == -1 : continue
      for t in range(N):
        tvar = self.tran_var.get( (index, t) )
        if not tvar: continue
        
        lst= []
        
        for op in self.prepost_conn_start[index]:
          var = self.oper_var.get(  (op, t) )
          if var: lst.append( var )

        for op in self.prepost_conn_end[index]:
          du = self.opers[op].duration
          var = self.oper_var.get( (op, t - du + 1) )
          if var: lst.append( var )
        
        s.new_clause_add_lit( -1 * tvar )
        for lit in lst: s.new_clause_add_lit( lit )
        s.new_clause_push()


    """
          Action -> all the transitions to be true
    """
    print "Actions => transitions...              ", s.nClauses()
    for op_i,oper in enumerate(self.task.opers):
      for t in range(0,N):
        ovar = self.oper_var.get( (op_i, t))
        if not ovar:  continue
        
        lst = []
        
        flag = False
        for tr_i in oper.overall:
            for t2 in range( t+1, t+oper.duration-1 ):
                var = self.tran_var.get((tr_i,t2))
                if var:  
                  lst.append( var )
                else:
                  flag = True
                  
        for tr_i in oper.start:
            var = self.tran_var.get((tr_i, t))
            if not var: flag = True
            lst.append( var )
        for tr_i in oper.end:
            var = self.tran_var.get((tr_i, t + oper.duration - 1))
            if not var: flag = True
            lst.append( var )
        
        if flag : continue 
        
        #Generate the clause
        for lit in lst:
            s.new_clause_add_lit( lit )
            s.new_clause_add_lit( -1 * ovar )
            s.new_clause_push()
    
    #i = self.trans_dict[1,1,1]
    #print "KKKK",self.tr_first_appr[i]
    #print "AAA", self.tran_var.get((i,5))
    """
        Transition's Regression
    """
    print "Transition Regression...               ", s.nClauses()
    for index,tr in enumerate(self.transitions):
        mv,pre,post = tr
        if pre == -1 : continue
        for t in range(1,N):
            tvar = self.tran_var.get( (index,t) )
            if not tvar: continue
            
            lst = []
            for pre2 in range(-1,self.ranges[mv]):
                tran2 = (mv,pre2,pre)
                j = self.trans_dict.get(tran2)
                if j!=None:
                    tvar2 = self.tran_var.get( (j,t-1) )                 
                    if tvar2:  lst.append(tvar2)
            
            s.new_clause_add_lit( -1 * tvar )
            for lit in lst:   s.new_clause_add_lit( lit )
            s.new_clause_push()

    """
        Transition's Progression
    """
    """
    for index,tr in enumerate(self.transitions):
        mv,pre,post = tr
        for t in range(0,N-1):    
            tvar = self.tran_var.get( (index,t) )
            if not tvar: continue
            
            lst = []
            for post2 in range(0,self.ranges[mv]):
                tran2 = (mv,post,post2)
                j = self.trans_dict.get(tran2)
                if j:
                    tvar2 = self.tran_var.get( (j,t+1) )
                    if tvar2:   lst.append( tvar2 )
                j = self.trans_dict.get((mv,-1,post2))
                if j: 
                    tvar2 = self.tran_var.get( (j,t+1) )
                    if tvar2:   lst.append( tvar2 )                  
            #if len(lst) > 0:
            s.new_clause_add_lit( -1 * tvar )
            for lit in lst:   s.new_clause_add_lit( lit )
            s.new_clause_push()
    """
    
    """
            Transition Mutex ; Normal Transitions
    """
    print "Transition Mutex...                    ", s.nClauses()    
    for t in range(N):
        for mv in range(len(self.ranges)):
            r = self.task.ranges[mv]
            lst = []
            for i,tr_i in enumerate(self.mv_trans[mv]):
                assert self.transitions[tr_i][1] != -1 
                var1 = self.tran_var.get((tr_i,t)) 
                if not var1: continue
                lst.append(var1)
                
                """
                for tr_i2 in self.mv_trans[mv][i+1:]:
                    var2 = self.tran_var.get((tr_i2,t))
                    if not var2: continue
                    
                    assert self.transitions[tr_i][1] != -1
                    assert self.transitions[tr_i2][1] != -1
                    assert self.transitions[tr_i][0] == self.transitions[tr_i2][0]
                    s.new_clause_add_lit( -1 * var1)
                    s.new_clause_add_lit( -1 * var2)
                    s.new_clause_push()
                """
            
            self.add_clique_constraints(s,lst)
            
    """
            Transition Mutex ; Mecanical Transitions
    """
    for t in range(N):
        for mv in range(len(self.ranges)):
            r = self.task.ranges[mv]
            for post in range(self.ranges[mv]):
                if (mv,-1,post) in self.trans_dict:
                    index = self.trans_dict[mv,-1,post]
                    var = self.tran_var.get( (index,t) )
                    if var == None: continue
                    for post2 in range(self.ranges[mv]):
                        if post2 == post: continue
                        for pre in range(-1, self.ranges[mv]):
                            index2 = self.trans_dict.get( (mv,pre,post2) )
                            if index2: 
                                var2 = self.tran_var.get( (index2,t) )
                                if not var2: continue
                                s.new_clause_add_lit( -1 * var)
                                s.new_clause_add_lit( -1 * var2)
                                s.new_clause_push()
                                    
    """
          Action Mutex
    """
    print "Encoding required action mutexes...    ", s.nClauses()
    for index,tr in enumerate( self.transitions ):
      if tr[1] == tr[2] or tr[1] == -1: continue
      
      for t in range(N):
        lst= []
        for op in self.prepost_conn_start[index]:
          var = self.oper_var.get(  (op, t) )
          if var:  lst.append( var )
        for op in self.prepost_conn_end[index]:
          du = self.opers[op].duration
          var = self.oper_var.get( (op, t - du) )
          if var:  lst.append( var )
        
        if len(lst) < 2: continue
        self.add_clique_constraints(s,lst)
    
    """
        Self Mutex
    """
    """
    print "Encoding self-mutexes...               ", s.nClauses()    
    for index,oper in enumerate(self.opers):
      for t in range(N):
        for t2 in range(t+1,t+oper.duration-1):
            var1 = self.oper_var.get( (index,t) )
            var2 = self.oper_var.get( (index,t2) )
            assert not var1 or not var2 or var1 != var2
            if var1 and var2 and var1 != var2:
                s.new_clause_add_lit( -1 * var1 )
                s.new_clause_add_lit( -1 * var2 )
                s.new_clause_push()
    """
    self.print_all_variables()
    s.dump_all_clause()
    
    var_cnt = s.nVars()
    clause_cnt = s.nClauses()

    print "Done with encoding. Now solve it...."
    rst = s.solve()
    print "Solving Done:", rst
    if rst == False:
      del s
      return []
    else:
      plan = self.decode( s, task, N )
      import resource
      all_time = resource.getrusage(resource.RUSAGE_SELF)[0]
      all_time += resource.getrusage(resource.RUSAGE_CHILDREN)[0]
      print "Make Span:", N
      print "Time Spent :", all_time
      print "Var:", var_cnt
      print "Clause:", clause_cnt

      del s 
      return plan
      
      
  #####################################################################
  def decode(self, solver, task, N ):
    print "Decoding for total time layer ", N
    DO_VERIFY = False
    
    #print "TRUTH:", solver.truth_assignments
    
    plan = [[] for item in range(N)]
    for t in range( N ):
      for index, oper in enumerate(self.opers):
        var = self.oper_var.get((index,t))
        if var == None: continue
        if var in solver.truth_assignments:
            plan[t].append(index)
    
    for i,p in enumerate(plan):
        print len(p),
    print 
    
    #Verify the plan; 
    marks = [False for item in range(N)]
    for t,p in enumerate(plan):
        for op_i in p:
            oper = self.opers[op_i]
            for i in range(t, t+oper.duration):
                if i > N -1 : 
                  print "Warning: time  %d seems out of bound %d " % (i, N-1)
                marks[i] = True
    
    if DO_VERIFY:
        for m in marks:
            if not m : 
                print "Not a valid plan"
                return []
    return plan      
### END OF ENCODING_CONSTANT class ###################################################
