#! /usr/bin/env python2.5
# -*- coding: latin-1 -*-
import os
import axiom_rules
import fact_groups
import instantiate
import pddl
import sas_tasks
import simplify
import sys
import copy
import SATSolver

def gcd(a,b):
  while b:      
    (a, b) = (b, a % b)
  return a


class TemporalSaseEncoding(object):
  
  def __init__(self,task):
    self.task = task
    self.op_var = {}  #Each key in the dictionary is a 2-tuple (op_index, time )
    self.ft_var = {}  #Each key in the dictionary is a 2-tuple (ft_index, time)
    self.ft_dict = {}
    self.op_dict = {}
    
    self.zero_action = [] #Zero Duration Action;  Handle them as special case!!!!!!
    self.opers = []
    self.facts =[]
    self.op_split = [[] for item in range(len(task.opers))]
    self.op_split_pf = ["" for item in range(len(task.opers))]
    self.op_overall_pre = [ [] for item in range(len(task.opers))]
    self.p_facts = []
    self.p_facts_lnk = []
    
    for oper in task.opers:
      oper.duration = int(oper.duration)
    
    self.facts.extend( task.facts )
    for index,oper in enumerate( task.opers ):    
      #Performing (persistently) fact for non-trivial durative actions;
      if oper.duration > 1 :
        per_fact = pddl.conditions.Atom("Perf:" + oper.name, [] )
        self.p_facts.append( per_fact )
        self.p_facts_lnk.append( index )
        self.op_split_pf[index] = per_fact
    self.facts.extend( self.p_facts )
    
    #Fact Dictionary
    for i, fact in enumerate(self.facts):
      self.ft_dict[fact]  = i
    #self.op_split_pf = [self.ft_dict[item] for item in self.op_split_pf]
    
    #Split the duration actions;
    for index,oper in enumerate( task.opers ):
      if oper.duration == 1 :
        self.opers.append( oper )
        self.op_split[index] =  len(self.opers) - 1 
      elif oper.duration == 0:
        self.zero_opers.append( oper )
      else:
        per_fact = self.op_split_pf[index]
        #1.Preprocessing...
        overall_pre = []
        start_pre = []
        start_eff = []
        end_pre   = []
        end_eff   = []
        for pre,time_type in oper.precondition:
          if time_type == "start":
            start_pre.append( (pre,"") )
          elif time_type == "end":
            end_pre.append( (pre,"end") )
          elif time_type == "overall":
            overall_pre.append( (pre,"") )
          else:
            assert False
        
        for cond, eff, time_type in oper.add_effects:
          if time_type == "start":
            start_eff.append( ("",eff , ""))
          elif time_type == "end":
            end_eff.append( ("",eff, "") )
          else:
            assert False
        
        for cond, eff, time_type in oper.del_effects:
          if time_type == "start":
            start_eff.append( ("",eff.negate(),"") )
          elif time_type == "end":
            end_eff.append( ("",eff.negate(),"") )
          else:
            assert False
        
        self.op_overall_pre[index].extend(overall_pre)
        
        #2. Construct Starting Action;
        start_eff.append( ("", per_fact, "") )
        start_a = pddl.actions.PropositionalAction\
                  ("S_"+oper.name, start_pre, start_eff, 1)  
        self.opers.append( start_a )
        
        #3.Construct Ending Action;
        end_eff.append( ("", per_fact.negate(), ""))
        end_pre.append( (per_fact,"") )
        #end_pre.extend( overall_pre )
        end_a = pddl.actions.PropositionalAction\
                  ("E_"+oper.name, end_pre, end_eff , 1) 
        self.opers.append( end_a )
        
        self.op_split[index] = (len(self.opers)-2, len(self.opers)-1 ) 
    
    #each element is a direct object to either an PropAction or Atom
    #each element is a tuple Fact: (index, time); Action: (index, TYPE{0,1,2}, Time );
    self.var_lnk = []
    self.var_cnt = 1
    
    #Add noop operators into task.opers;
    self.noops   = []    
    for fact in self.facts:
      precond = []
      effects = []
      precond.append((fact,""))              #Time_type doesn't matter
      if fact in self.p_facts:
        index = self.p_facts.index( fact )
        precond.extend(self.op_overall_pre[self.p_facts_lnk[index]])  
      effects.append(([],fact,""))
      noop = pddl.actions.PropositionalAction( "Noop" + fact.__str__(), precond, effects, 1 )
      self.noops.append(noop)
    self.opers.extend(self.noops)
    
    #Building Dictionaries
    for i, oper in enumerate(self.opers):
      self.op_dict[oper] = i
    
    self.ft_conn_add  = [[] for item in range(len(self.facts))]
    self.ft_conn_del  = [[] for item in range(len(self.facts))]
    #Build ft-op connections
    for oper_index, oper in enumerate(self.opers):
      if oper.duration <1:
        assert False

      for cond, add, time_type in oper.add_effects:
        assert add in self.ft_dict and self.ft_dict[add] <= len(self.facts)
        self.ft_conn_add[self.ft_dict[add]].append( oper_index )
      for con, de, time_type in oper.del_effects:
        assert de in self.ft_dict and self.ft_dict[de] <= len(self.facts)
        self.ft_conn_del[self.ft_dict[de]].append( oper_index )

    for index in range(len(self.facts)):
      self.ft_conn_add[index] = list(set(self.ft_conn_add[index]))
      self.ft_conn_del[index] = list(set(self.ft_conn_del[index]))
    
    #**To Compute Required Action-Mutexes
    self.req_op_mutex = [set() for item in range(len(self.opers))]   #duo-tuples in this list    
    del_eff_list = [set() for item in range(len(self.opers))]
    add_eff_list = [set() for item in range(len(self.opers))]
    prec_list = [set() for item in range(len(self.opers))]
    pneg_list = [set() for item in range(len(self.opers))]
    for index,oper in enumerate(self.opers):
      del_eff_list[index] = set([self.ft_dict[item] for d,item,t in oper.del_effects])
      add_eff_list[index] = set([self.ft_dict[item] for d,item,t in oper.add_effects])
      for pre,tt in oper.precondition:
        if pre.negated:
          pneg_list[index].add( self.ft_dict[pre.negate()] )
        else:
          prec_list[index].add( self.ft_dict[pre])

    for index in range(len(self.opers)):
      del_ = del_eff_list[index]
      add_ = add_eff_list[index]
      prec_ = prec_list[index]
      pneg_ = pneg_list[index]
      for index2 in range(index+1,len(self.opers)):
        del_2 = del_eff_list[index2]
        add_2 = add_eff_list[index2]
        prec_2 = prec_list[index2]
        pneg_2 = pneg_list[index2]
        if len( del_ & add_2  ) != 0 or len( add_ & del_)!=0\
          or len( prec_ & del_2 )!= 0 or len( prec_2 & del_)!=0 \
          or len( add_ & pneg_2 )!=0 or len( add_2 & pneg_ ):
          self.req_op_mutex[index].add(index2)
    
    print "Encoding Constant Initilized!"
    
  #Returns an integer
  def get_op_var( self, op_i, time ):
    if type(op_i)!=type(1):
      op_i = self.op_dict[op_i]
    if (op_i, time ) in self.op_var:
      return int( self.op_var[(op_i, time)] )
    else:
      if self.op_min[op_i] > self.op_max[op_i]:
        return -1
      elif time >= self.op_min[op_i] and time <= self.op_max[op_i]:
        self.op_var[(op_i,time)] = self.var_cnt
        self.var_lnk.append( ("o", op_i, time))
        self.var_cnt+=1
        return self.var_cnt - 1
      else:
        return -1

  #Returns an integer
  def get_ft_var( self, ft_i , time):
    if type(ft_i) != type(1):
      ft_i = self.ft_dict[ft_i]
    if (ft_i, time) in self.ft_var:
      return self.ft_var[(ft_i,time)]
    else:
      self.ft_var[(ft_i,time)] = self.var_cnt
      self.var_lnk.append( ("f", ft_i, time ) )
      self.var_cnt += 1
      return self.var_cnt-1

  
  def decode(self, solver, task, N ):
    #solver.print_raw_result()
    
    #Debug
    for index,oper in enumerate(task.opers):
      o_tuple = self.op_split[index]
      if type(o_tuple) == type(1):
        continue
      s_op = self.op_split[index][0]
      e_op = self.op_split[index][1]
      for t in range(N):
        has_s = (s_op,t) in self.op_var
        has_e = (e_op,t+oper.duration-1) in self.op_var
        assert has_e == has_e, "S_op and E_op not consistent"
        
    #End of Debug
    
    PRINT_WITH_PERF_OPS = True
    print "Decoding for total time layer ", N
    
    plan = [[] for item in range(N)]
    for t in range( 0, N ):
      for index,o_oper in enumerate(task.opers):
        o_tuple = self.op_split[index]
        if type(o_tuple) == type(1):
          #not a duration action;
          assert ( o_tuple , t ) in self.op_var
          var = self.op_var[(o_tuple,t)]
          rst = solver.model.element(var-1).get_v()
          if rst == 1:
            plan[t].append(index)
        elif len(o_tuple) == 2:
          s_op = o_tuple[0]
          if (s_op,t) in self.op_var:
            var = self.op_var[(s_op, t)]
            rst = solver.get_assignment(var)
            if rst == 1:
              plan[t].append(index)
        else:
          assert False

    #For debug, print out all the states
    print "#################################"
    for t in range(0,N+1):
      print "Time",t,":"
      for index,fact in enumerate(self.facts):
        if (index,t) in self.ft_var:
          var = self.ft_var[(index,t)]
          #rst = solver.model.element(var-1).get_v()
          rst = solver.get_assignment(var)
          if rst == 1:
            print fact.__str__()[4:] + ";" ,
      print ""
    print "\n#################################"
    
    if task.domain_name == "p2p":
      for fact in task.goal.parts:
        fid = self.ft_dict[fact]
        flag = len(plan)
        for i in range(len(plan)):
          if plan[i] in self.ft_conn_add[fid]:
            flag = i
            break
        for j in range(i,len(plan)):
          for addf in self.ft_conn_add[fid]:
            if addf in plan[j]:
              plan[j].remove(addf)
    return plan
    
  #This N means N layers of actions!!!!!!!!!!!!!!!!!!
  def solve_decision( self, task,  N):
    s = SATSolver.Solver()
    
    self.op_min = [0 for item in self.opers]
    self.op_max = [N-1 for item in self.opers]
    for index,oper in enumerate(task.opers):
      if type(self.op_split[index]) == type(1):
        continue
      ops = self.op_split[index][0]
      ope = self.op_split[index][1]
      self.op_max[ops] = N - int(oper.duration)
      self.op_min[ope] = int(oper.duration) - 1

      """
      if self.op_min[ops] > self.op_max[ops]:
        print 'Operator %s, with duration %d cannot be fit into a %d-Span plan.' % ( oper.name, int(oper.duration), N )
      """
    
    """
          #1. initial state and goal state
    """
    print "Solving with make span:", N
    print "Encoding init,goal..."
    #1.1
    for fact in task.init:
      if fact not in self.ft_dict:
        continue
      index = self.ft_dict[fact]
      var = self.get_ft_var(  index, 0  )
      s.new_clause_add_lit( var )
      s.new_clause_push()
    
    #1.2
    for fact in self.facts:
      if fact in task.init or fact not in self.facts:
        continue
      index = self.ft_dict[fact]
      var = self.get_ft_var(index,0)
      s.new_clause_add_lit( -1 * var )
      s.new_clause_push()
    
    #1.3
    for fact in task.goal.parts:
      index = self.ft_dict[fact]
      var = self.get_ft_var( index, N  )
      s.new_clause_add_lit( var ) 
      s.new_clause_push()


    """
          Action -> all the preconditions must be true;
                 -> all the del-effects must be true
    """
    print "Encoding action => preconditions..."
    for index,oper in enumerate(self.opers):
      #print "Action", self.opers[index].name, " implies"    
      for t in range(0,N):
        var = self.get_op_var( index, t)
        if var == -1 :
          continue
        
        #Conditions
        #if "S_" not in oper.name and "E_" not in oper.name and "Noop" not in oper.name:
        #  oper.dump()
        #  sys.exit()
        for pre,time_type in oper.precondition:
          s.new_clause_add_lit( -1 * var )
          if pre.negated :
            var2 = self.get_ft_var( self.ft_dict[pre.negate()], t )
            s.new_clause_add_lit( -1 * var2 )
          else:
            #print "     fact",pre.__str__()
            var2 = self.get_ft_var( self.ft_dict[pre], t )
            s.new_clause_add_lit( var2 )
          s.new_clause_push()
        
        #Del-effects
        #if t == N-1:
        #  continue
        for cond,de,time_type in oper.del_effects:
          s.new_clause_add_lit( -1 * var )
          var2 = self.get_ft_var( self.ft_dict[de], t+1 )
          s.new_clause_add_lit( -1 * var2 )
          s.new_clause_push()

    """
          Fact -> action's effect
    """
    print "Encoding fact => adding actions..."    
    for index,fact in enumerate( self.facts ):
      for t in range(1,N+1):
        var = self.get_ft_var( index, t )
        s.new_clause_add_lit( -1 * var )
        #print "Fact", fact.__str__(), "implies", len(self.ft_conn_add[index])
        for add_op in self.ft_conn_add[index]:
          #print "     ",add_op, self.opers[add_op].name
          var2 = self.get_op_var( add_op, t-1 )
          if var2 != -1:
            s.new_clause_add_lit( var2 )
        s.new_clause_push()

    """
          Structural Durative Action info
    """
    print "Encoding durative info for actions..."
    for index,op_tuple in enumerate( self.op_split ):
      if type(op_tuple) == type(1) :
        continue
      duration = int(task.opers[index].duration)        
      so_index = op_tuple[0]
      eo_index = op_tuple[1]
      pf_index = self.ft_dict[self.op_split_pf[index]]
      
      assert type(so_index) == type(eo_index) == type(pf_index) == type(1)
      assert task.opers[index].name in self.op_split_pf[index].__str__()
      assert task.opers[index].name in self.opers[so_index].name
      assert task.opers[index].name in self.opers[eo_index].name
      
      for st in range(0,N):
        et = st + duration - 1
        
        ###  Equivalence Relationship between Start_o and End_o
        s_var =  self.get_op_var( so_index, st )
        e_var =  self.get_op_var( eo_index, et )
        if s_var == -1 or e_var == -1 :
          #print "CONTINUE because no varaible defined"
          continue
        s.new_clause_add_lit( -1 * s_var )
        s.new_clause_add_lit( e_var )
        s.new_clause_push()
        s.new_clause_add_lit( -1 * e_var )
        s.new_clause_add_lit( s_var  )
        s.new_clause_push()
      
        ###  Equivalence Relationship betwee Start_o and Per_f        
        for per_t in range(st+1,et):
          pf_var = self.get_ft_var( pf_index, per_t )
          s.new_clause_add_lit( -1 * s_var )
          s.new_clause_add_lit( pf_var )
          s.new_clause_push( )
        
      # Perf_t implies Start_o to be true in a range;
      # Note:  Perf cannot be true at time $N
      #for perf_t in range(0,N):
      #  pf_var = self.get_ft_var( pf_index, per_t )
    """
          Axioms!!!!!!!!!!!!!!! to-do:
    """
    print "Encoding axioms..."    
    for axiom in task.gaxioms:
      for t in range(N+1):
        lit = []
        if axiom.effect.negated:
          index = self.ft_dict[axiom.effect.negate()]
          var = self.get_ft_var(index,t)
          s.new_clause_add_lit( -1 * var )        
        else:
          index = self.ft_dict[axiom.effect]
          var = self.get_ft_var(index,t)
          s.new_clause_add_lit( var )
        for ft in axiom.condition:
          if ft.negated:
            ft_i = self.ft_dict[ft.negate()]
            var = self.get_ft_var(ft_i,t)
            s.new_clause_add_lit( var2 )          
          else:
            ft_i = self.ft_dict[ft]
            var = self.get_ft_var(ft_i,t)
            s.new_clause_add_lit( -1 * var2 )
        s.new_clause_push()

    """
          Required action Mutex
    """
    print "Encoding required action mutexes..."        
    for index in range(len(self.opers)):
      for index2 in self.req_op_mutex[index]:
        for t in range(0,N):
          if (index,t) in self.op_var and (index2,t) in self.op_var:
            s.new_clause_add_lit( -1 * self.op_var[(index,t)] )
            s.new_clause_add_lit( -1 * self.op_var[(index2,t)] )
            s.new_clause_push( )

    """
          Redundent Fact Mutex
    """
    print "Encoding additional fact mutexes..."        
    for group in task.ft_groups:
      for i in range(len(group)):
        if group[i] not in self.ft_dict:
          continue
        index = self.ft_dict[group[i]]
        for i2 in range(i+1,len(group)):
          if group[i2] not in self.ft_dict:
            continue        
          index2 = self.ft_dict[group[i2]]
          for t in range(0,N):
            if (index,t) in self.ft_var and (index2,t) in self.ft_var:
              s.new_clause_add_lit( -1 * self.ft_var[(index,t)] )
              s.new_clause_add_lit( -1 * self.ft_var[(index2,t)] )
              s.new_clause_push( )
    
    s.max_var = self.var_cnt + 1
    self.nvars = s.nVars()
    self.nclauses = s.nClauses()
    
    #Output the action-cost information
    COST_FILE = open("COST_" + str(os.getpid()), "w")
    for index,op_tuple in enumerate( self.op_split ):
      for st in range(0,N):
        s_var = self.get_op_var( op_tuple[0], st)
        if s_var == -1 or task.opers[index].cost == 0:
          continue
        
        print >> COST_FILE, s_var ,
        print >> COST_FILE, task.opers[index].cost
    COST_FILE.close()

    print "Done with encoding. Now solve it...."
    rst = s.solve()
    print "Solving Done:", rst
    if rst == False:
      del s
      return []
    else:
      plan = self.decode( s, task, N )
      del s 
      return plan
### END OF ENCODING_CONSTANT class ###################################################
