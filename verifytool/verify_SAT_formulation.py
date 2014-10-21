#!/usr/bin/python
import re,os,sys,sets
from sets import Set

class Verifier:
  def __init__(self):
    self.true_var = Set()
    self.false_var = Set()
    self.formula = []
    self.satisfied_clauses = Set()
    self.implied_reason = {}
    self.verification_failed = Set()
    
  def read_formula(self):
    file = open("formula.verify","r")
    lines = file.readlines()
    for line in lines:
      #dt = re.compile(r"[\S]+(\[\S\]+)")
      if line.strip() == "" :
        continue
      a = line.split(":")[1].strip()
      clause = []
      #print a.split(" ")
      for item in a.split(" "):
        clause.append(int(item))
      self.formula.append( clause )

    #print formula
  
  def add_to_knowledgebase(self,var):
    assert var != 0  
    self.updated = True
    if var>0:
      self.true_var.add(var)
    else:
      self.false_var.add(abs(var))
      
  def read_setting(self):
    file = open("truth.verify","r")  
    lines = file.readlines()
    for line in lines:
      line = line.strip()
      if line == "":
        continue
      ints = line.split(" ")
      for item in ints:
        if item.strip() == "":
          continue
          
        if ":" in item:
          begin = int(item.split(":")[0])
          end   = int(item.split(":")[1])
          if begin > end:
            temp = begin
            begin = end
            end = temp
          for item in range(begin,end+1):
            self.add_to_knowledgebase(int(item))
        else:
          self.add_to_knowledgebase(int(item))

    print self.true_var
    print self.false_var
    assert len(self.true_var.intersection(self.false_var))==0
  
  def verify_clause(self,index,clause):

    satisfied = 0
    undecided = []
    violations = []
    for lit in clause:
      if lit > 0:
        if abs(lit) in self.true_var:
          satisfied += 1
          break
        elif abs(lit) in self.false_var:
          violations.append(lit)
        else:
          undecided.append(lit)
          
      else:
        if abs(lit) in self.false_var:
          satisfied += 1
          break
        elif abs(lit) in self.true_var:
          violations.append(lit)
        else:
          undecided.append(lit)
      
    if satisfied != 0 :
      return True
      
    if satisfied == 0 and len(undecided) == 1 and (len(violations) == len(clause)-1):
      lit = undecided[0]
      self.add_to_knowledgebase(lit)
      self.implied_reason[abs(lit)] = (index+1,clause)
      return True
    
    if satisfied == 0 and len(violations) !=0 and len(undecided) == 0 :
      if index not in self.verification_failed:
        print "Verification Failed on ",index+1,"th clause:", clause, "due to ", violations
        self.verification_failed.add( index )
    return False
        

########################  Parsing ended #################
 
  def do_verification(self):
    while True:
      self.updated = False
      for index,clause in enumerate(self.formula):
        if index not in self.satisfied_clauses:
          rst = self.verify_clause(index,clause)
          if rst :
            self.satisfied_clauses.add(index)
      if not self.updated:
        break
  
if __name__ == "__main__":
  
  v = Verifier()
  v.read_formula()
  v.read_setting()
  
  v.do_verification()
  print "----------"
  p = list(v.true_var)
  p.sort()
  print "Truth:",p
  p = list(v.false_var)
  p.sort()
  print "False:",p
  

  #All the undertermined clauses:
  """
  for index,clause in enumerate(v.formula):
    if index not in v.satisfied_clauses:
      print index+1," : ", clause
  """
  

  check_var = 65
  print "The reason for %d is:" % (check_var)
  if check_var > 0 :
    if  check_var in v.implied_reason.keys():
      print v.implied_reason[check_var]
    else:
      print check_var, " is pre-set !!!!!!!!!!!!!!!!!!!!!"
