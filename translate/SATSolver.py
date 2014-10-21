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

TIME_LIMIT = 3600
SOLVER_NAME = "precosat" 
SOLVER_NAME2 = "sat-scan" 


class Solver(object):
  def __init__(self):
    self.clauses= []
    self.lits = []
    self.max_var = 0
    
  def new_clause_add_lit(self, lit ):
    self.lits.append( lit )
    if abs( lit ) > self.max_var:
      self.max_var = abs(lit)

  def new_clause_push(self ):
    self.lits.sort()
    self.clauses.append( self.lits )
        
    self.lits = []
    
  def dump_all_clause(self):
    cnt = 0
    out = open( "debug.clauses", "w")    
    for clause in self.clauses:
      print >> out, cnt, " : ", 
      for lit in clause:  print >> out, lit,
      print >> out, ""
      cnt += 1
          
  def solve(self):
    
    CNF_FILE = "CNF_" + str(os.getpid())
    COST_FILE = "COST_"+ str(os.getpid())
    RST_FILE = "sat.result" + str(os.getpid())
    
    out = open( CNF_FILE, "w")
    print >> out, "p cnf", self.max_var, len(self.clauses)
    for clause in self.clauses:
      for lit in clause:  print >> out, lit,
      print >> out, "0"
    del self.clauses
    out.close()
    
    #Run Precosat!
    PRECOSAT_PATH = "./" + SOLVER_NAME
    if not os.path.exists(PRECOSAT_PATH ):
      PRECOSAT_PATH = "../" + SOLVER_NAME 
    run_str = "ulimit -t %d; %s %s %s"%(TIME_LIMIT, PRECOSAT_PATH, CNF_FILE, RST_FILE)
    #run_str = "ulimit -t %d; %s -c %s -s %s -o %s "%(TIME_LIMIT, PRECOSAT_PATH, CNF_FILE, COST_FILE, RST_FILE)
    print run_str
    os.system(run_str)
    
    if not os.path.exists( RST_FILE ):
      print "Time Out!!! Fail to Solve in %d sec" % ( TIME_LIMIT )
      os.remove(CNF_FILE)
      sys.exit()
    
    #to-do: commented just for debugging
    #os.remove(CNF_FILE)
    #os.remove(COST_FILE)
    file = open( RST_FILE, "r")
    line = file.readline().strip()
    self.truth_assignments = set()
    if line == "UNSATISFIABLE":
      os.remove(RST_FILE)
      return False
    else:
      #sys.exit()
      while True:
        line = file.readline()
        if not line :
          break
        line = line.strip()
        if line == "":
          continue
        vals = line.split(" ")
        for i in vals:
          self.truth_assignments.add( int(i) )
      return True
      os.remove( RST_FILE )

  def get_assignment(self,var):
    if var in self.truth_assignments:
      return True
    else:
      return False


  def nClauses(self):
    return len(self.clauses)
  
  def nVars(self):
    return self.max_var

  
  def print_solving_result(self):
    
    print "SOLVING RESULT:"
    for i in range(self.nVars()):
      rst = self.solver.model.element(i)
      if rst == None:
        print "Failed on ", i
        assert False
      rst = rst.get_v()
      if rst == 1:
        print (i+1),
      else:
        print -1 * (i+1), 
    print 
