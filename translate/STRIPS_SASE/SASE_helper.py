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
import resource

#CONFIGURABLE RUNNING OPTIONS
#PORT_PLANNING_GRAPH_INFO = True     #Deprecated
DO_PLANNING_GRAPH_ANALYSIS = True


  
################################################
class SolvingSetting(object):

  def __init__(self):
    self.start_layer = 1
    self.domain_file = ""
    self.prob_file   = ""
    self.gdomain_file = ""
    self.gprob_file   = ""
    self.info         = 0
    self.encoding     = 1 
    self.dump = 0

################################################

#Helper Functions:
def cartesian_product(L,*lists):
    if not lists:
        for x in L:
            yield (x,)
    else:
        for x in L:
            for y in cartesian_product(lists[0],*lists[1:]):
                yield (x,)+y


def cartesian_product_list(lists, previous_elements = []):
    if len(lists) == 1:
        for elem in lists[0]:
            yield previous_elements + [elem, ]
    else:
        for elem in lists[0]:
            for x in cartesian_product_list(lists[1:], previous_elements + [elem, ]):
                yield x

def print_transition(tran):
  print "%d:%d->%d" % (tran[0],tran[1],tran[2])
  
  
def print_clause_progress(s,solver):
  s = s + " " * (55 - len(s))
  s += str(solver.nClauses())
  print s
  
def dec2bin(i):
  b = ''
  while i > 0:
    j = i & 1
    b = str(j) + b
    i >>= 1
  return b
  
  
def duo_enum(list1, list2):
    rst = []
    for item1 in list1:
      for item2 in list2:
        rst.append((item1,item2))
    return rst

def tri_enum(list1,list2,list3):
    rst = []
    for item1 in list1:
      for item2 in list2:
        for item3 in list3:
          rst.append( (item1,item2,item3))
    return rst



def print_all_axioms(task):
  for axiom in task.axioms:
    axiom.dump()

def print_all_op(task):
  for op in task.opers:
    pass
  
def print_sasop(operator):
  for pre_post in operator:
    pass


def get_pre_set(op):
  rst = set()
  for mv,pre,post,cond in op.pre_post:
    rst.add( (mv,pre) )
  for mv,val in op.prevail:
    rst.add( (mv,val) )
  return rst


#Build a planning graph & Get first_appearance info
#This procedure could be significantly improved!
def build_pg_info(encoding):
  task = encoding.task
  
  encoding.first_appearance = [0 for i in range(len(encoding.task.operators))]
  encoding.first_appearance_FT = {}
  
  if not DO_PLANNING_GRAPH_ANALYSIS:
    return

  translation_key_dict = {}
  for var_no, var_key in enumerate(encoding.task.translation_key):
      for value, value_name in enumerate(var_key):
        if value_name[0:4] == "Atom":
          translation_key_dict[value_name[5:]] = (var_no,value)
  
  if not DO_PLANNING_GRAPH_ANALYSIS:
    return

  # First, build up conns
  op_repository = set(range(len(task.operators)))
  ft_repository = set()
  ft_dict = {} 
  cnt = 0
  for mv in range(len(task.variables.ranges)):
    for i in range(task.variables.ranges[mv]):
      ft_dict[(mv,i)] = cnt
      cnt += 1
      ft_repository.add((mv,i))
  op_first_execute = {}
  op_preds = [set() for i in range(len(task.operators))]

  for op_i, op in enumerate(task.operators):
    for mv, pre, post, cond in op.pre_post:
      if pre!=-1:
        op_preds[op_i].add( (mv,pre) )
    for mv,val in op.prevail:
      op_preds[op_i].add( (mv,val) )

  #Major Routine
  pg_ft_layer = []
  pg_op_layer = []
  
  layer = set()
  for mv,val in enumerate(task.init.values):
    layer.add( (mv,val))
    encoding.first_appearance_FT[mv,val] = 0
    ft_repository.remove((mv,val))
  pg_ft_layer.append(layer)

  time = 0
  while True:
    pg_op_layer.append( set() )

    #1. Copy the facts to the next fact layer
    curr_ft_layer = pg_ft_layer[time]
    next_ft_layer = set()

    #2. Execute the appropriate actions;
    new_op_found = False
    to_remove = []
    for op_i in op_repository:
      flag = True
      for pred in op_preds[op_i]:
        if pred not in curr_ft_layer :
          flag = False
          break
      if flag:
        assert encoding.first_appearance[op_i] == 0
        new_op_found = True
        encoding.first_appearance[op_i] = time
        for mv,pre,post,cond in task.operators[op_i].pre_post:
          if (mv,post) in ft_repository:
            encoding.first_appearance_FT[mv,post] = time + 1
            ft_repository.remove((mv,post))
          next_ft_layer.add( (mv,post) )
        to_remove.append(op_i)
     
    for op_i in to_remove:
      op_repository.remove( op_i )
    
    if not new_op_found:
      break

    next_ft_layer = next_ft_layer.union( curr_ft_layer )
    time += 1
    pg_ft_layer.append( next_ft_layer )
  
   
  del pg_ft_layer
  del pg_op_layer



################################################
#Deprecated!!
def parse_pgraph_info(encoding):

  gdomain = ""
  gtask   = ""
  
  gdomain = sys.argv[1]
  gtask   = sys.argv[2]

  flag = False
  for i,ag in enumerate(sys.argv):
    if ag[0] == '-':
      if ag[1:] == 'gd':
        gdomain = sys.argv[i+1]
      elif ag[1:] == 'gt':
        gtask   = sys.agv[i+1]
      elif ag[1:] == 'adl' or ag[1:]=='convert':
        flag = True

  """ Convert """
  if flag:
    convert_str = "adl2strips-linux-static -o " + gdomain + " -f " + gtask
    os.system(convert_str)
    gdomain = "./domain.pddl"
    gtask   = "./facts.pddl"
    
  run_str = ""
  if os.path.exists('./SatPlan2006/include/bb'):
    run_str = "./SatPlan2006/include/bb "
  elif os.path.exists('../SatPlan2006/include/bb'):
    run_str = "../SatPlan2006/include/bb "
  run_str +=  " -o " + gdomain + " -f " + gtask  
  
  infile = None
  if os.path.exists('./Graph_Info.output.debug'):
    infile = open("Graph_Info.output.debug","r")
  else:
    os.system(run_str)
    infile = open("Graph_Info.output","r")

  print "Start parsing planning graph info...",
  start_time = resource.getrusage(resource.RUSAGE_SELF)[0]  
  
  ###########################  For FTs ########################
  encoding.first_appearance_FT = {}
  translation_key_dict = {}
  
  for var_no, var_key in enumerate(encoding.task.translation_key):
      for value, value_name in enumerate(var_key):
        if value_name[0:4] == "Atom":
          translation_key_dict[value_name[5:]] = (var_no,value)
  
  ft_cnt = int(infile.readline())
  ft_mapping = [-1 for i in range(ft_cnt)]  
  for i in range(ft_cnt):
    line = infile.readline()
    ln = line.split(" ")
    ln[-1] = ln[-1][0:-1]
    id = int(str(ln[0]))
    first_app = int(ln[1])
    if first_app == -1:
      first_app = sys.maxint
    name = ", ".join(ln[2:]).lower()
    
    if name in translation_key_dict:
      var,val = translation_key_dict[name]
      encoding.first_appearance_FT[(var,val)] = first_app
      ft_mapping[id] = (var,val)

  ###########################  For OP ########################

  op_cnt = int(infile.readline())  
  op_mapping = [-1 for i in range(op_cnt)]
  encoding.first_appearance = [0 for op in encoding.task.operators]
  compared_op = set()
  for i in range(op_cnt):
    line = infile.readline()
    ln = line.split(" ")
    ln[-1] = ln[-1][0:-1]
    id = int(str(ln[0]))
    first_app = int(ln[1])
    if first_app == -1:
      first_app = sys.maxint
    name = " ".join(ln[2:]).lower()
    name = "(" + name + ")"
    
    found = False
    for index in range(len(encoding.task.operators)):
      if index in compared_op:
        continue
      formatted_name = ""
      if "_" in name:
        formatted_name = "_".join(encoding.task.operators[index].name.split(" "))
      else:
        formatted_name = encoding.task.operators[index].name
      
      if name == formatted_name:
        op_mapping[i] = index
        compared_op.add(index)
        found = True
        encoding.first_appearance[index] = first_app
        break
        
    if not found:
      continue  #Not found
 
  print resource.getrusage(resource.RUSAGE_SELF)[0] - start_time, "s done."
  return


def print_all_variables(encoding):
  for var in range(encoding.var_cnt-1):
    t = encoding.var_lnk[var] 
    if t[0] == "trans":
      if len(t) == 5:
        print var+1, "is var[%d:%d->%d@%d]" % (t[1],t[2],t[3],t[4])
      elif len(t) == 4:
        print var+1, "is var_bit[%d:%d_th@%d]" % (t[1],t[2],t[3])
    elif t[0] ==  "op":
      print var+1, "is op[%d:%s@%d]" %(t[1], encoding.task.operators[t[1]].name, t[2])
    elif t[0] == "enft":
      print var+1, "is enft[%d:ALL->%d], at time %d" % (t[1],t[2],t[3]) 
    else:
      pass

#Debug
if __name__ == "__main__":
  s = SolvingSetting()

  list = [(1,2,3),(4,5,6)]
  list2 = [[1,2,3]]
  for comb in cartesian_product_list(list2):
    print comb
