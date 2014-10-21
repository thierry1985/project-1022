#! /usr/bin/env python
# -*- coding: latin-1 -*-

import invariants
import pddl

class BalanceChecker(object):
  def __init__(self, task):
    self.predicates_to_add_actions = {}
    for action in task.actions:
      for eff in action.effects:
        if isinstance(eff.peffect,pddl.Atom):
          predicate = eff.peffect.predicate
          self.predicates_to_add_actions.setdefault(predicate, set()).add(action)
    for action in task.durative_actions:
      for timed_effects in action.effects:
        for eff in timed_effects:
          if isinstance(eff.peffect,pddl.Atom):
            predicate = eff.peffect.predicate
            self.predicates_to_add_actions.setdefault(predicate, set()).add(action)
  def get_threats(self, predicate):
    return self.predicates_to_add_actions.get(predicate, set())

def get_fluents(task):
  fluent_names = set()
  for action in task.actions:
    for eff in action.effects:
      if isinstance(eff.peffect,pddl.literal):   
        fluent_names.add(eff.peffect.predicate)
  for action in task.durative_actions:
    for timed_effects in action.effects:
      for eff in timed_effects:
        if isinstance(eff.peffect,pddl.Literal):   
          fluent_names.add(eff.peffect.predicate)
  return [pred for pred in task.predicates if pred.name in fluent_names]

def get_initial_invariants(task):
  for predicate in get_fluents(task):
    all_args = range(len(predicate.arguments))
    for omitted_arg in [-1] + all_args:
      order = [i for i in all_args if i != omitted_arg]
      part = invariants.InvariantPart(predicate.name, order, omitted_arg)
      yield invariants.Invariant((part,))

def find_invariants(task):
  
  candidates = list(get_initial_invariants(task))

  seen_candidates = set(candidates)

  balance_checker = BalanceChecker(task)

  def enqueue_func(invariant):
    if invariant not in seen_candidates:
      candidates.append(invariant)
      seen_candidates.add(invariant)

  while candidates:
    candidate = candidates.pop()
    if candidate.check_balance(balance_checker, enqueue_func):
      yield candidate

"""   TFD's group finding
def useful_groups(invariants, initial_facts):
  predicate_to_invariants = {}
  for invariant in invariants:
    for predicate in invariant.predicates:
      predicate_to_invariants.setdefault(predicate, []).append(invariant)

  nonempty_groups = set()
  overcrowded_groups = set()
  for atom in initial_facts:
    if not isinstance(atom,pddl.FunctionAssignment):
      for invariant in predicate_to_invariants.get(atom.predicate, ()):
        group_key = (invariant, tuple(invariant.get_parameters(atom)))
        if group_key not in nonempty_groups:
          nonempty_groups.add(group_key)
        else:
          overcrowded_groups.add(group_key)
  useful_groups = nonempty_groups - overcrowded_groups
  for (invariant, parameters) in useful_groups:
    yield [part.instantiate(parameters) for part in invariant.parts]
"""


def useful_groups(task, invariants, initial_facts):
  predicate_to_invariants = {}

  for invariant in invariants:
    for predicate in invariant.predicates:
      predicate_to_invariants.setdefault(predicate, []).append(invariant)

  nonempty_groups = set()
  overcrowded_groups = set()
  for atom in initial_facts:
    if atom.__class__.__name__ == "Assign": continue
    for invariant in predicate_to_invariants.get(atom.predicate, ()):
      group_key = (invariant, tuple(invariant.get_parameters(atom)))
      if group_key not in nonempty_groups:
        nonempty_groups.add(group_key)
      else:
        overcrowded_groups.add(group_key)
  
  if task.domain_name == "toy":
    print "R.Huang Hacking for better SAS+ rep.", len(nonempty_groups)
    
    print len(nonempty_groups)
    #R.Huang to-do
    #for item in nonempty_groups:
    #    print "KKKK", item
    
    #import sys
    #sys.exit()
  elif task.domain_name == "matchlift":
    print len(nonempty_groups)
    #for i in nonempty_groups:
    #  print str(i[0])
    
  useful_groups = nonempty_groups - overcrowded_groups
  for (invariant, parameters) in useful_groups:
    yield [part.instantiate(parameters) for part in invariant.parts]

def get_groups(task):
  invariants = find_invariants(task)
  #R.Huang
  #print "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
  #for item in invariants:
  #  print item
  #import sys
  #sys.exit()
  #End of revision;

  rst = list(useful_groups(task, invariants, task.init))
  """
  for item in rst:
    print "ZZ:",
    for i in item:
      print i,
    print 
  """
  return rst
  
if __name__ == "__main__":
  import pddl
  print "Parsing..."
  task = pddl.open()
  print "Finding invariants..."
  for invariant in find_invariants(task):
    print invariant
  print "Finding fact groups..."
  groups = get_groups(task)
  for group in groups:
    print "[%s]" % ", ".join(map(str, group))
