#! /usr/bin/env python2.5
# -*- coding: latin-1 -*-

import axiom_rules
import fact_groups
import instantiate
import pddl
import sas_tasks
import simplify
import sys
import copy
import os
import SASE_helper
from SASE_helper import *
import math

#DEBUGGING OPTIONS
USE_SATPLAN06_ANALYSIS = False


################################################

class BaseSASEncoding(object):
  
  """   @Debug Code  501 """
  def process_first_appearance_info(self):
    """   This procedure call may need to be somewhere else """
    PROC_DEBUG = False
    assert len(self.trans_lst) == len(self.trans_dict)
    
    if  USE_SATPLAN06_ANALYSIS:
      SASE_helper.parse_pgraph_info(self)
    else:
      SASE_helper.build_pg_info(self)
    
    if self.status.info == 501 :
      for index in range(len(self.task.operators)):
        print self.task.operators[index].name, " --FA--> ", self.first_appearance[index]
    
    self.first_apr_trans = [0 for tran in self.trans_lst]
    for item,time in self.first_appearance_FT.items():
      mv,val = item
      pres = set(self.pres[mv][val])
      assert val in pres
      for tran_val in pres:
        if (mv,val,tran_val) in self.trans_dict:
          index = self.trans_dict[mv,val,tran_val]
          if self.first_apr_trans[index] <  time:
            self.first_apr_trans[index] = time 
      
      posts = set( self.posts[mv][val] )
      assert val in posts
      for tran_val in posts:
        if (mv,tran_val,val) in self.trans_dict:
          index = self.trans_dict[mv,tran_val,val]
          if self.first_apr_trans[index] < time:
            self.first_apr_trans[index] = time - 1
    
    for index,op in enumerate(self.task.operators):
      for mv,pre,post,cond in op.pre_post:
        tran_time = self.first_apr_trans[self.trans_dict[mv,pre,post]]
        if self.first_appearance[index] < tran_time:
          self.first_appearance[index] = tran_time
      for mv,val in op.prevail:
        tran_time = self.first_apr_trans[self.trans_dict[mv,val,val]]
        if self.first_appearance[index] < tran_time:
          self.first_appearance[index] = tran_time
        
    if self.status.info == 501 :
      for item,time in self.first_appearance_FT.items():
        print item,time, ";",
      print
      for i,time in enumerate(self.first_apr_trans):
        print i,time, ";", 
      print
      
      for trans,index in self.trans_dict.items():
        apr_trans = sys.maxint
        
        if index < self.useful_trans_cnt:
          for op in self.trans_conn[index]:
            if self.first_appearance[op] < apr_trans:
              apr_trans = self.first_appearance[op]
      sys.exit()  
  
  
  """ @Debug Code 502 """
  def process_analyze_trans_last_appear(self):
    self.dist_to_N = [0 for i in self.trans_lst]  
    self.trans_graph = [None for i in self.mv_ranges] 
    for i,r in enumerate(self.mv_ranges):
      self.trans_graph[i] = [[False for k in range(r)] for j in range(r)]
    
    for op in self.task.operators:
      for mv,pre,post,cond in op.pre_post:
        if pre != -1:
          self.trans_graph[mv][pre][post] = True
        else:
          for i in range(self.mv_ranges[mv]):
            self.trans_graph[mv][i][post] = True

    self.dist_to_N = [0 for i in self.trans_lst]
    for mv,val in self.task.goal.pairs:
      visited = set([val])
      curr_distance = 0
      frontier = set([val])
      while True:
        exploring = set()
        for i in range(0,self.mv_ranges[mv]):
          if i in visited:
            continue
          
          for j in frontier:
            if self.trans_graph[mv][i][j] and (mv,i,j) in self.trans_dict:
              exploring.add(i)
              index = self.trans_dict[mv,i,j]
              self.dist_to_N[index] = curr_distance
              break
        for i in exploring:
          visited.add(i)
        curr_distance += 1
        frontier = set( exploring )
        if len(frontier)==0:
          break
        
    if self.status.info == 502:
      print "Reduction of transitions, backward"
      for index,tran in enumerate(self.trans_lst):
        if self.dist_to_N[index] == 0:
          continue
        print "%d:%d->%d" % tran,
        print self.dist_to_N[index]
      sys.exit()
  
  
  
  """  @Debug Code 503 """
  """  Finds out subsumed transition and most simple ops """
  def process_subsumed_trans_info(self):
    translation_key_dict = {}
    if self.status.info == 503:
      for var_no, var_key in enumerate(self.task.translation_key):
          for value, value_name in enumerate(var_key):
            if value_name[0:4] == "Atom":
              translation_key_dict[var_no,value] = value_name[5:]
            elif value_name.strip() == "<none of those>":
              translation_key_dict[var_no,value] = "var"+str(var_no)+"-"+str(value)+"()"
            else:
              assert False  

    self.op_conn = [[] for i in self.task.operators]
    for op_i,op in enumerate(self.task.operators):
      for mv,pre,post,cond in op.pre_post:
        self.op_conn[op_i].append( self.trans_dict[mv,pre,post] )
      for mv,val in op.prevail:
        self.op_conn[op_i].append( self.trans_dict[mv,val,val] )
    for i in range(len(self.op_conn)):
      self.op_conn[i].sort()
    
    self.trans_implication = {}
    self.subsumed_trans = set() 
    cnt = 0
    for index,item in enumerate(self.trans_lst[0:self.useful_trans_cnt]):
      if item[1] != item[2]:
        assert len(self.trans_conn[index]) != 0 
        op_lst = list(self.trans_conn[index])
        tran_set = set(self.op_conn[op_lst[0]])
        
        assert index in tran_set
        tran_set.remove(index)
        for op in op_lst[1:]:
          tran_set = tran_set.intersection( set(self.op_conn[op]) )

        if (len(tran_set)!=0):
          self.trans_implication[index] = tran_set
          self.subsumed_trans = self.subsumed_trans.union(tran_set)
          cnt += len(tran_set)
    
    #The conflict effect makes this semantics incorrect, skip it for rovers
    if "rover" in self.task.domain_name:
      self.subsumed_trans = []
    
    #for index in range(self.useful_trans_cnt):
    #  if index in self.trans_implication:
    #    set1 = set(self.trans_conn[index])
    #    for index2 in range(index+1,self.useful_trans_cnt):
    #      if index2 in self.trans_implication and len(set1) == set(self.trans_conn[index2]):
    #        set2 = set(self.trans_conn[index2])
    #        if set1 == set2:
    #          del self.trans_implication[index2] 
    

    #R. Huang, Bug fixing, 8/27/2010
    self.clique_first_required_time = [0 for i in range(self.useful_trans_cnt)]
    self.unnecessary_cliques = set()
    self.fixed_cliques = set()
    for index in range(self.useful_trans_cnt):
      #to-do: This is awkward, 
      #the bug is gone by the following, but not quite sure why
      if index in self.fixed_cliques:
        continue
      for index2 in range(index+1,self.useful_trans_cnt):
        if self.trans_conn[index].issubset(self.trans_conn[index2]):
          self.unnecessary_cliques.add(index)
          self.fixed_cliques.add(index2)
          #self.clique_first_required_time[index] = tmp_first_clique_time[index2]
          #assert tmp_first_clique_time[index2] != -1
          break
    #End of Bug fixing


    print "Implication Set: %d/%d" % (len(self.trans_implication),len(self.trans_lst)),
    print " Total Subsumed Trans #", len(self.subsumed_trans), " useful for saving op mutex"

    if self.status.info == 503:
      print "Useful Transitions", self.useful_trans_cnt
      
      cnt = 0
      all = 0
      for index,s in self.trans_implication.items():
        if len(self.trans_conn[index])==1:
          continue
        cnt += len( self.trans_conn[index])
        all += 1
      avg = 1
      if all != 0 :
        avg = float(cnt) / all
      print "Reducible cnt",all,"Avg op in Transitions",avg
      
      cnt = 0 
      num = 0
      for index in range(self.useful_trans_cnt):
        if len(self.trans_conn[index])==1:
          continue
        num += 1
        cnt += len( self.trans_conn[index])
      avg = float(cnt) / num
      print "all cnt", num, "Avg op in all Transitions", avg

      sys.exit()


  #@Debug Code 504
  def process_eqv_trans_sets(self):
   
    #to-do We may need to study it again later.
    self.equivalent_trans = []
    self.equivalent_trans_mapping = {}
    
    if self.status.info == 504:
      debug_set = set()
      for index,item in enumerate(self.trans_lst[0:self.useful_trans_cnt]):
        if index in self.trans_implication:
          trans_set = self.trans_implication[index]
          for elem in trans_set:
            if elem in self.trans_implication:
              if index in self.trans_implication[elem]:
                debug_set.add((index,elem))
      print debug_set
    

    #If two transition subsume each other, then they are equivalent
    for index,item in enumerate(self.trans_lst[0:self.useful_trans_cnt]):
      if index in self.trans_implication:
        trans_set = self.trans_implication[index]
        gid1 = -1
        if index in self.equivalent_trans_mapping:
          gid1 = self.equivalent_trans_mapping[index]
        for elem in trans_set:
          if elem in self.trans_implication:
            gid2 = -1
            if elem in self.equivalent_trans_mapping:
              gid2 = self.equivalent_trans_mapping[elem]
             
            if index in self.trans_implication[elem]:
              
              if gid1==-1 and gid2==-1:
                tmpset = set([index,elem])
                self.equivalent_trans.append(tmpset)
                self.equivalent_trans_mapping[index] = len(self.equivalent_trans) - 1 
                self.equivalent_trans_mapping[elem] = len(self.equivalent_trans) - 1 
                gid1 =  len(self.equivalent_trans) - 1
              elif gid1!=-1 and gid2!=-1 and gid1!=gid2:
                #Merge it!
                gid = min(gid1,gid2)
                to_del = max(gid1,gid2)
                for item in list(self.equivalent_trans[to_del]):
                  self.equivalent_trans[gid].add( item )
                self.equivalent_trans[to_del] = set()
                for item in list(self.equivalent_trans[gid]):
                  self.equivalent_trans_mapping[item] = gid
                gid1 = self.equivalent_trans_mapping[index]
              elif gid1!= -1 or gid2!=-1:
                gid = gid1
                if gid == -1:
                  gid = gid2
                assert gid <= len(self.equivalent_trans)
                self.equivalent_trans[gid].add(index)
                self.equivalent_trans[gid].add(elem)
                self.equivalent_trans_mapping[index] = gid
                self.equivalent_trans_mapping[elem] = gid
              else:
                pass
          #print self.equivalent_trans
          #print self.equivalent_trans_mapping
          
    print "Equivalent Set cnt:", len(self.equivalent_trans), " out of ", len(self.trans_lst)

    if self.status.info == 504:
      cnt = 0
      covered_trans = set()
      for st in self.equivalent_trans:
        for item in st:
          assert item not in covered_trans
          covered_trans.add( item )

      print self.equivalent_trans
      print "Transition Coverage %d/%d" %( len(covered_trans)-len(self.equivalent_trans), self.useful_trans_cnt)
      sys.exit()



  """ @Debug Code 505 """
  def find_reducible_ops_basic(self):
    print "Basic reducible op analysis..."
    self.reduced_op_dict = {}
    reduced_trans_set = set() 
    for index,item in enumerate(self.trans_lst[0:self.useful_trans_cnt]):
      if item[1] != item[2] and len(self.trans_conn[index]) == 1:
        #if list(self.trans_conn[index])[0] in self.simple_op_dict:
        #  if self.simple_op_dict[list(self.trans_conn[index])[0]] != index: #Already in the list
        #    print "OOOPS, seems more reduction is possible!!!!!!!!!!!!!!"
        #    sys.exit()
        self.reduced_op_dict[list(self.trans_conn[index])[0]] = [index]
        reduced_trans_set.add( index )

    print "Simple Reducible Trans ", len(reduced_trans_set), " out of total", self.useful_trans_cnt
    print "Simple Reducible Op ", len(self.reduced_op_dict), " out of total ", len(self.task.operators)
    
    if self.status.info == 505:
      #For Debug 
      len_cnt = [0 for i in range(6)]
      for index,tran in enumerate(self.trans_lst[0:self.useful_trans_cnt]):
        l = len(self.trans_conn[index])
        if l > 5:
          len_cnt[5] += 1
        else:
          len_cnt[l-1] += 1
      print "Consensus of #actions in transitions:", len_cnt
      sys.exit()


  """ @Debu Code 506 """
  def find_reducible_ops_advanced(self):
    print "More advanced reducible op analysis..."
    self.status.easy_op_threshold = 32  # This is just a luck number for threashold;
    unary_diff_cnt = 0
    easy_diff_cnt = 0
    easy_diff_op_set = set()
    adv_reducible_ops = {}
    adv_reducible_trans = set()
    self.shared_preconditions = {}
    self.diff_degree = [-1 for i in range(self.useful_trans_cnt)]
    
    for index,item in enumerate(self.trans_lst[0:self.useful_trans_cnt]):
      if item[1] != item[2] and len(self.trans_conn[index]) != 0:
        op_lst = list(self.trans_conn[index])
        if len(op_lst) <= 1 :
          continue
        
        tran_set = set(self.op_conn[op_lst[0]])
        assert index in tran_set
        #tran_set.remove(index) #Note: We should leave this alone!! Not necessary!
        for op in op_lst[1:]:
          tran_set = tran_set.intersection( set(self.op_conn[op]) )
        if len(tran_set)!=0:
          self.shared_preconditions[index] = tran_set

        self.diff_degree[index] = 1
        for op in op_lst:
          if op in self.reduced_op_dict:
            continue
          assert len(self.op_conn[op]) >= len(tran_set)
          diff = set(self.op_conn[op]) - tran_set
          
          if len(diff)!=0:
            self.diff_degree[index] *= len(diff)
            if self.diff_degree[index] > self.status.easy_op_threshold:
              break
          else:
            self.diff_degree[index] = 0
            break

        #Record the discoveries;
        if self.diff_degree[index] == 1:
          flag = False
          for op in op_lst:
            if op in self.reduced_op_dict:
              continue
            item = set(self.op_conn[op]) - tran_set
            assert len(item) == 1
            item = list(item)[0]
            #self.reduced_op_dict[op] = [index,item]
            adv_reducible_ops[op] = [index,item]
            adv_reducible_trans.add(index)
            flag = True
          if flag:
            unary_diff_cnt += 1
        elif self.diff_degree[index] == 0: 
          print "Warning! A zero diff found when analyzing!"
          pass
        elif self.diff_degree[index] <= self.status.easy_op_threshold:
          easy_diff_cnt += 1
          if self.status.info == 506:
            for op in self.trans_conn[index]:
              easy_diff_op_set.add(op)
        else:
          pass
    
    if self.status.encoding == 1:
      for op,item in adv_reducible_ops.items():
        t1,t2 = item
        if t1 in adv_reducible_trans and t2 in adv_reducible_trans:
          self.reduced_op_dict[op] = item
    print "%d Single Diff, %d Easy, out of total %d," % (unary_diff_cnt,easy_diff_cnt, self.useful_trans_cnt),
    print "SinDiff Covered Actions %d/%d" %( len(adv_reducible_ops), len(self.task.operators)) 
    print "Reasonable Reducible covered actions %d", len(easy_diff_op_set)
    print "# of shared Preconditions ", len(self.shared_preconditions)
    
    if self.status.info == 506:
      cnt  = 0
      for index,item in self.shared_preconditions.items():
        cnt += len(item)
      print "SHARED PRECONDITION relation cnt", cnt
      sys.exit()

  #The translator have problem in correctly parse conflicting effects!!
  #Therefore, we handle Rover, where the issue occurs, as a special case;
  def handle_conflicting_effects(self):
    conflict_eff_cliques = []
    split_char = " "
    
    self.conflict_eff_cliques = []
    obj_ref = {}
    if "rover" in self.task.domain_name:
      print "Rover Domain Found!"
      for index,op in enumerate(self.task.operators):
        if op.name.startswith("(communicate_"):
          rover = op.name.split(split_char)[1]
          if rover in obj_ref:
            ref = obj_ref[rover]
            conflict_eff_cliques[ref].append(index)
          else:
            last = len(conflict_eff_cliques)
            conflict_eff_cliques.append([])
            obj_ref[rover] = last
            conflict_eff_cliques[last].append(index)
          
          channel = op.name.split(split_char)[2]
          if channel in obj_ref:
            ref = obj_ref[channel]
            conflict_eff_cliques[ref].append(index)
          else:
            last = len(conflict_eff_cliques)
            conflict_eff_cliques.append([])
            obj_ref[channel] = last
            conflict_eff_cliques[last].append(index)

    self.conflict_eff_cliques = conflict_eff_cliques

  def update_trans_repo(self,mv,pre,post):
    index = -1
    if (mv,pre,post) in self.trans_dict:
      index = self.trans_dict[(mv,pre,post)]
    else:
      self.trans_dict[(mv,pre,post)] = len(self.trans_lst)
      index =  len(self.trans_lst)
      self.trans_lst.append( (mv,pre,post) )
    
    assert index != -1
    self.mv_trans[mv].add( (pre,post) )
    if pre!= -1:
      self.pres[mv][pre].add( post )
    self.posts[mv][post].add( pre )
    return index
      
  
  """
    self.mv_ranges[]   scope of each variables
    self.num_mv      number of variables
  After the initilization, important data structures will be:
    1. self.mv_ranges[]:
    2. self.pre_trans[set(),set()]
    3. self.trans_conn
    4. self.mv_trans
  """
    
  def __init__(self,sas,status):
    self.status     = status
    self.mv_ranges  = sas.variables.ranges
    self.num_mv     = len(sas.variables.ranges)
    self.task       = sas
    self.special_mv = set()  #Normal mv with '-1' nodes
    self.special_mv_var = set()
    self.axiom_mv = set()
    
    ####################              Part.1                ###################
    #Verify all the assumptions we made in the processing
    if sas.translation_key != None:
      for var_no, var_key in enumerate(sas.translation_key):
          
        flag = False
        for value,value_name in enumerate(var_key):
          if value_name == "<none of those>":
            self.special_mv_var.add((var_no,value))
            flag = True
            break
        
        if flag:
          flag2 = False
          for value,value_name in enumerate(var_key):
            if "new-axiom@" in value_name:
              flag2 = True
              self.axiom_mv.add(var_no)
            if not flag2:
              self.special_mv.add( var_no )
    
    for op_i,op in enumerate(sas.operators):
      for i,pre_post in enumerate(op.pre_post):
        if pre_post[1] == -1 or pre_post[2]==-1:
          pass
          #assert self.mv_ranges[pre_post[0]] == 2  ####Assumption!!!!!!
      for i,item in enumerate(op.prevail):
        assert item[1] != -1
    #End of verification;
    
    
    ####################              Part.2                ###################
    self.mv_trans   = [set() for item in range(self.num_mv)]
    self.pres       = [[] for item in range(self.num_mv)]
    self.posts      = [[] for item in range(self.num_mv)]
    self.trans_dict = {}
    self.trans_lst  = [] 
    
    for var in range(self.num_mv):
      self.pres[var]    = [set() for item in range(self.mv_ranges[var])]
      self.posts[var]   = [set() for item in range(self.mv_ranges[var])]
     
    #@Update: 
    #  self.trans_dict, self.mv_trans, self.pres, self.posts, self.trans_cnt
    for op_i,op in enumerate(sas.operators):
      for var, pre, post, cond in op.pre_post:
        index = self.update_trans_repo( var, pre, post  )
      for var, val in op.prevail:
        index = self.update_trans_repo( var, val, val  )
        
    #A mapping from trans to ops
    self.trans_conn = [set() for item in range(len(self.trans_dict))]
    
    #Add all the possible trans from operators' definitions
    for op_i,op in enumerate(sas.operators):
      for var, pre, post, cond in op.pre_post:
        assert len(cond) == 0
        assert post != -1
        
        #if pre == -1 :
        #  for val in self.posts[var][post]:
        #    index = self.trans_dict[(var,val,post)]
        #    self.trans_conn[index].add( op_i )
        #else:
        index = self.trans_dict[(var,pre,post)]
        self.trans_conn[index].add( op_i )
      for var, val in op.prevail:
        index = self.trans_dict[(var,val,val)]
        self.trans_conn[index].add( op_i )
    
    ####For Debug
    if self.status.info == 500:
      #Print out all the initialized data structures.
      for var in range(len(self.mv_ranges)):
        print "VAR:",var
        for i in range(self.mv_ranges[var]):
          print "  VAL:",i, 
          print "  PRES:", self.pres[var][i],
          print "  POST:", self.posts[var][i]
      for trans,index in self.trans_dict.items():
        print "|%d:%d->%d|" % ( trans[0], trans[1], trans[2]), 
        for op in self.trans_conn[index]:
          print " [%s] " % sas.operators[op].name , 
        print
      print "Axiom MVs",self.axiom_mv
      print "Special Variables", self.special_mv
      sys.exit(0)
    
    
    #These functions below might be over-riden by each specific encoding's own logic
    self.encoding_specific_processing()
    self.process_first_appearance_info()
    self.process_analyze_trans_last_appear()
    
    self.process_subsumed_trans_info()
    self.process_eqv_trans_sets()
    
    self.find_reducible_ops_basic()
    self.find_reducible_ops_advanced()
    self.handle_conflicting_effects()  # Hack for Rovers, Pathway and few other domains
    print "Number of Transitions:", len(self.trans_dict)
    print "Number of Actions:",len(self.task.operators)    
    print "Encoding-Constants Initilized!"
    
    if self.status.info == 510:
      self.analyze_op_mutex()

  def get_starting_time_step(self):
    rst = 0
    for mv,val in self.task.goal.pairs:
      single = sys.maxint
      for possible_pre in range(-1,self.mv_ranges[mv]):
        if (mv,possible_pre,val) in self.trans_dict:
          index = self.trans_dict[mv,possible_pre,val]
          single = min( single, self.first_apr_trans[index])
      assert single != sys.maxint
      rst = max(single,rst)
    return rst

  def encoding_post_operations(self,vs,s):
    if "max_var" in dir(s):  #This is a hack!!!
      s.max_var = self.var_cnt + 1        
    self.nvars = s.nVars()
    self.nclauses = s.nClauses()
    
  #To Be Over-ridden
  def encoding_specific_processing(self):
    print "Not supposed to enter specific pre-processing in Base Encoding"
    assert False
    pass
    
  
  """ @Debug Code 510 """
  # After this procedure, op_conn will be different to the original data structure!!!!!
  def analyze_op_mutex(self):
    
    if self.status.info == 510: 
      print "Before Reduction:"
      cnt,total,L_cnt,S_cnt,I_cnt = [0] * 5
      for index in range(self.useful_trans_cnt):
        trans = self.trans_lst[index]
        l = len(self.trans_conn[index])
        if trans[1]!=trans[2] and l >1:
          cnt += 1
          total += l
          if l > 15:
            L_cnt += 1
          elif l > 3:
            S_cnt += 1
          else:
            I_cnt += 1
      print "# of clique| avg size | L(>16)|  S(>3) |  T  "
      print "     %d    | %0.2f    | %d    |  %d    | %d  " % (cnt, float(total)/float(cnt) , L_cnt, S_cnt, I_cnt )
      import math
      print "Estimated OP_MUTEX Size: ", cnt + math.log(float(total)/float(cnt)) 
  
    #BEGIN OF TEST CODE
    test_cnt =0
    for trans,index in self.trans_dict.items():
      if trans[1]==trans[2] or len(self.trans_conn[index]) == 1:
        continue
      
      main = set(self.trans_conn[index])
      check_set = set()
      for op in self.trans_conn[index]:
        for t in self.op_conn[op]:
          if self.trans_lst[t][1] != self.trans_lst[t][2] and len(self.trans_conn[t])> 1:
            check_set.add(t)
      if index in check_set:
        check_set.remove(index)

      for s in check_set:
        if main.issubset(set(self.trans_conn[s])):
          test_cnt += 1
          print "ZZ",len(main)
          break
    print "TEST CONT", test_cnt, len(self.trans_conn)
    sys.exit()
    #END OF TEST CODE
    
    trans_rdct = [ [] for i in range(len(self.trans_conn))]
    rdct_set   = []
    trans_conn = self.trans_conn[:]
    rdct_set_cnt = []    # Not Critical;
    
    #Round 1; Normal Cases;
    for trans,index in self.trans_dict.items():
      if trans[1]==trans[2] or len(trans_conn[index]) == 1:
        continue
      
      main = set(trans_conn[index])
      check_set = set()
      for op in trans_conn[index]:
        for t in self.op_conn[op]:
          if self.trans_lst[t][1] != self.trans_lst[t][2] and len(trans_conn[t])> 1:
            check_set.add(t)
      if index in check_set:
        check_set.remove(index)
      
      max_id = -1
      max_set = set()
      for t in list(check_set):
        if len(trans_rdct[t])==0:
          set2 = set(trans_conn[t])
          if len(set2) < 4:
            continue
          rst = main.intersection( set2 )
          if len(rst)>len(max_set):
            max_id = t
            max_set = rst
      
      if max_id != -1 and len(max_set) > 1:
        rdct_set.append(max_set)
        rdct_set_cnt.append(0)
        #if len(trans_rdct[max_id])==0 and len(trans_rdct[max_id])==0:
        j = len(rdct_set) -1
        trans_rdct[max_id].append( j )
        trans_rdct[index].append(j)
        rdct_set_cnt[j] += 2
        
        trans_conn[max_id] -= max_set
        trans_conn[index] -= max_set
    
    #Round 2, for especially long sequences;
    round2_cnt = 0
    for trans,index in self.trans_dict.items():
      if trans[1]==trans[2] or len(trans_conn[index]) <16:
        continue
      
      main_set = set(trans_conn[index])
      if len(trans_rdct[index]) !=0 :
        ref = trans_rdct[index]
        main_set -= rdct_set[ref[0]]
      
      for i,rset in enumerate(rdct_set):
        if i != ref:
          if rset.issubset(main_set):
            trans_rdct[index].append(i)
            main_set = main_set - rset
            round2_cnt += 1
      
    if self.status.info == 510:
      for trans,index in self.trans_dict.items():
        if trans[1]!=trans[2] or len(trans_conn[index]) > 1:
          for i in trans_rdct[index]:
            rdct_set_cnt[i] += 1
      
      print "After Reduction:"
      cnt,total,L_cnt,S_cnt,I_cnt = [0] * 5
      for index in range(self.useful_trans_cnt):
        trans = self.trans_lst[index]
        if trans[1]!=trans[2] and len(self.trans_conn[index]) >1:
          cnt += 1
          after_reduced = set(self.trans_conn[index])
          for rs in trans_rdct[index]:
            after_reduced -= set(rdct_set[rs])
          l = len(after_reduced)            
          total += len(after_reduced) + len(trans_rdct[index])
          if l > 15:
            L_cnt += 1
          elif l > 3:
            S_cnt += 1
          else:
            I_cnt += 1
      
      for s in rdct_set:
        l = len(s)
        cnt += 1
        total += l
        if l > 15:
          L_cnt += 1
        elif l > 3:
          S_cnt += 1
        else:
          I_cnt += 1 
      
      print "# of clique| avg size | L(>16)|  S(>3) |  T  "
      print "     %d    | %0.2f    | %d    |  %d    | %d  " % ( cnt, float(total)/float(cnt) , L_cnt, S_cnt, I_cnt )
      import math
      print "Estimated OP_MUTEX Size: ", cnt + math.log(float(total)/float(cnt)) 
      
      for s in rdct_set:
        print len(s),
      print rdct_set_cnt
      print "Number of common subset discover by Phase2", round2_cnt
      sys.exit()


  def report_encoding_stats(self):
    trans_cnt = 0
    op_cnt = 0
    aux_cnt = 0
    enft_cnt = 0
    
    for item in self.var_lnk:
      if item[0] == "trans":
        trans_cnt += 1
      elif item[0] == "op":
        op_cnt += 1
      elif item[0] == "AUX":
        aux_cnt += 1
      else: 
        assert False
        
    print "#Trans:",trans_cnt
    print "#Op:",op_cnt
    print "#AUX:",aux_cnt
    print "#ENFT:",enft_cnt


### END OF ENCODING_CONSTANT class ###################################################
