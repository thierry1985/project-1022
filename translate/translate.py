#! /usr/bin/env python
# -*- coding: latin-1 -*-
import sys

import axiom_rules
import fact_groups
import instantiate
import numeric_axiom_rules
import pddl
import sas_tasks
import simplify

# TODO: The translator may generate trivial derived variables which are always true,
# for example if there ia a derived predicate in the input that only depends on
# (non-derived) variables which are detected as always true.
# Such a situation was encountered in the PSR-STRIPS-DerivedPredicates domain.
# Such "always-true" variables should best be compiled away, but it is
# not clear what the best place to do this should be. Similar
# simplifications might be possible elsewhere, for example if a
# derived variable is synonymous with another variable (derived or
# non-derived).

ALLOW_CONFLICTING_EFFECTS = False
USE_PARTIAL_ENCODING = True
WRITE_ALL_MUTEXES = True

def strips_to_sas_dictionary(groups, num_axioms, num_fluents):
    dictionary = {}

    # sort groups to get a deterministic output
    map(lambda g: g.sort(lambda x, y: cmp(str(x),str(y))),groups)
    groups.sort(lambda x, y: cmp((-len(x),str(x[0])),(-len(y),str(y[0]))))
            
    for var_no, group in enumerate(groups):
        for val_no, atom in enumerate(group):
            dictionary.setdefault(atom, []).append((var_no, val_no))
    if USE_PARTIAL_ENCODING:
        assert all(len(sas_pairs) == 1
                   for sas_pairs in dictionary.itervalues())
    for var_no,axiom in enumerate(num_axioms):
        dictionary.setdefault(axiom.effect,[]).append((var_no + len(groups), -2))
   
    ranges = [len(group) + 1 for group in groups] + [-1]*len(num_axioms)
    
    var_no = len(groups) + len(num_axioms)
    fluent_list = list(num_fluents)
    fluent_list.sort(lambda x,y: cmp(str(x), str(y)))
    for fluent in fluent_list: # are partially contained in num_axiom
        if fluent not in dictionary:
            dictionary.setdefault(fluent,[]).append((var_no, -2))
            var_no += 1
            ranges.append(-1)
        
    return ranges, dictionary

def translate_strips_conditions(conditions, dictionary, ranges, comp_axioms, 
                                temporal=False):
    if temporal:
        condition = [translate_strips_conditions_aux(conds, dictionary, ranges, comp_axioms) 
                     for conds in conditions] 
        c = [item for item in condition if item!=None]
        #R.Huang
        if None in condition:
            return None
        else:
            return condition
        return c
    else:
        return translate_strips_conditions_aux(conditions, dictionary, ranges, comp_axioms)


def translate_strips_conditions_aux(conditions, dictionary, ranges, comparison_axioms):
    if not conditions:
        return {} # Quick exit for common case.

    condition = {}
    for fact in conditions:
        if (isinstance(fact,pddl.FunctionComparison) or 
            isinstance(fact,pddl.NegatedFunctionComparison)):
            if fact not in dictionary:
                if fact.negated:
                    negfact = fact
                    posfact = fact.negate()
                else:
                    posfact = fact
                    negfact = fact.negate()
                dictionary.setdefault(posfact,[]).append((len(ranges), 0))
                dictionary.setdefault(negfact,[]).append((len(ranges), 1))
                parts = [dictionary[part][0][0] for part in fact.parts]
                axiom = sas_tasks.SASCompareAxiom(fact.comparator, parts, len(ranges)) 
                comparison_axioms.append(axiom)
                ranges.append(3)
            var, val = dictionary[fact][0]
            if condition.get(var) not in (None, val):
                # Conflicting conditions on this variable: Operator invalid.
                return None
            condition[var] = val 
        else:
            atom = pddl.Atom(fact.predicate, fact.args) # force positive
            for var, val in dictionary[atom]:
                if fact.negated:
                    ## TODO!
                    ## BUG: Here we take a shortcut compared to Sec. 10.6.4
                    ##      of the thesis and do something that doesn't appear
                    ##      to make sense if this is part of a proper fact group.
                    ##      Compare the last sentences of the third paragraph of
                    ##      the section.
                    ##      We need to do what is written there. As a test case,
                    ##      consider Airport ADL tasks with only one airport, where
                    ##      (occupied ?x) variables are encoded in a single variable,
                    ##      and conditions like (not (occupied ?x)) do occur in
                    ##      preconditions.
                    ##      However, *do* what we do here if this is a binary
                    ##      variable, because this happens to be the most
                    ##      common case.
                    val = ranges[var] - 1
                if condition.get(var) not in (None, val):
                    # Conflicting conditions on this variable: Operator invalid.
                    return None
                condition[var] = val
    return condition

def translate_operator_duration(duration, dictionary):
    sas_durations = []
    for timed_duration in duration:
        timed_sas_durations = []
        for dur in timed_duration:
            var, val = dictionary.get(dur[1])[0]
            timed_sas_durations.append(sas_tasks.SASDuration(dur[0],var))
        sas_durations.append(timed_sas_durations)
    return sas_durations

def mutex_conditions(cond_dict, condition, temporal):
    # return value True means that the conditions are mutex
    # return value False means that we don't know whether they are mutex
    if temporal:
        for time in range(3):
            for var,val in condition[time]:
                if var in cond_dict[time]:
                    if cond_dict[time][var] != val:
                        return True
    else:
        for var,val in condition:
            if var in cond_dict:
                if cond_dict[var] != val:
                    return True
    return False

def implies(condition, condition_list, global_cond, temporal):
    # True: whenever condition is true also at least one condition 
    # from condition_list is true (given global_cond)
    if temporal:
        if [[],[],[]] in condition_list:
            return True
        for cond in condition_list:
            triggers = True
            for time in range(3):
                for (var,val) in cond[time]:
                    if (var,val) not in condition[time] and global_cond[time].get(var)!=val:
                        triggers=False
                        break
                if not triggers:
                    break
            if triggers:
                return True
    else:
        if [] in condition_list:
            return True
        for cond in condition_list:
            triggers = True
            for (var,val) in cond:
                if (var,val) not in condition and global_cond.get(var)!=val:
                    triggers=False
                    break
            if triggers:
                return True
    return False

def translate_add_effects(add_effects, dictionary, ranges, comp_axioms, temporal=False):
    effect = {}
    possible_add_conflict = False

    for conditions, fact in add_effects:
        eff_condition_dict = translate_strips_conditions(conditions, dictionary, 
                                         ranges, comp_axioms, temporal)
        if eff_condition_dict is None: # Impossible condition for this effect.
            continue

        if temporal:
            eff_condition = [eff_cond.items() for eff_cond in eff_condition_dict]
        else:
            eff_condition = eff_condition_dict.items()
        for var, val in dictionary[fact]:
            hitherto_effect = effect.setdefault(var,{})
            for other_val in hitherto_effect:
                if other_val != val:
                    for other_cond in hitherto_effect[other_val]:
                        if not mutex_conditions(eff_condition_dict, 
                                                other_cond, temporal):
                            possible_add_conflict = True
            hitherto_effect.setdefault(val,[]).append(eff_condition)
    return effect, possible_add_conflict

def translate_del_effects(del_effects,dictionary,ranges,effect,condition,
                          comp_axioms, temporal=False, time=None):
    if temporal:
        assert time is not None

    for conditions, fact in del_effects:
        eff_condition_dict = translate_strips_conditions(conditions, dictionary,
                                                  ranges, comp_axioms, temporal)
        if eff_condition_dict is None:
            continue

        if temporal:
            eff_condition = [eff_cond.items() for eff_cond in eff_condition_dict]
        else:
            eff_condition = eff_condition_dict.items()
        
        for var, val in dictionary[fact]:
            none_of_those = ranges[var] - 1
            hitherto_effects = effect.setdefault(var,{})
            
            # Look for matching add effect; ignore this del effect if found
            found_matching_add_effect = False
            uncertain_conflict = False

            for other_val, eff_conditions in hitherto_effects.items():
                if other_val!=none_of_those:
                    if implies(eff_condition, eff_conditions, condition, temporal):
                        found_matching_add_effect = True
                        break
                    for cond in eff_conditions:
                        if not mutex_conditions(eff_condition_dict, 
                                                cond, temporal): 
                            uncertain_conflict = True
            if found_matching_add_effect:
                continue
            else:
                assert not uncertain_conflict, "Uncertain conflict"
                if temporal:
                    if (condition[time].get(var) != val and 
                       eff_condition_dict[time].get(var) != val):
                        # Need a guard for this delete effect.
                        assert (var not in condition[time] and 
                               var not in eff_condition[time]), "Oops?"
                        eff_condition[time].append((var, val))
                else:
                    if condition.get(var) != val and eff_condition.get(var) != val:
                        # Need a guard for this delete effect.
                        assert var not in condition and var not in eff_condition, "Oops?"
                        eff_condition.append((var, val))
                eff_conditions = hitherto_effects.setdefault(none_of_those,[])
                eff_conditions.append(eff_condition)

def translate_assignment_effects(assign_effects, dictionary, ranges, comp_axioms, 
                                 temporal=False):
    effect = {}
    possible_assign_conflict = False

    for conditions, assignment in assign_effects:
        eff_condition_dict = translate_strips_conditions(conditions, dictionary, 
                                         ranges, comp_axioms, temporal)
        if eff_condition_dict is None: # Impossible condition for this effect.
            continue

        if temporal:
            eff_condition = [eff_cond.items() for eff_cond in eff_condition_dict]
        else:
            eff_condition = eff_condition_dict.items()
        for var, dummy in dictionary[assignment.fluent]:
            for expvar, dummy in dictionary[assignment.expression]:
                val = (assignment.symbol, expvar)
                hitherto_effect = effect.setdefault(var,{})
                for other_val in hitherto_effect:
                    if other_val != val:
                        for other_cond in hitherto_effect[other_val]:
                            if not mutex_conditions(eff_condition_dict, 
                                                    other_cond, temporal):
                                possible_assign_conflict = True
                hitherto_effect.setdefault(val,[]).append(eff_condition)
    return effect, possible_assign_conflict

def translate_strips_operator(operator, dictionary, ranges, comp_axioms):
    # NOTE: This function does not really deal with the intricacies of properly
    # encoding delete effects for grouped propositions in the presence of
    # conditional effects. It should work ok but will bail out in more
    # complicated cases even though a conflict does not necessarily exist.

    condition = translate_strips_conditions(operator.condition, dictionary, ranges, comp_axioms)
    if condition is None:
        return None

    effect, possible_add_conflict = translate_add_effects(operator.add_effects, 
                                                          dictionary, ranges, comp_axioms)
    translate_del_effects(operator.del_effects,dictionary,ranges,effect,condition, comp_axioms)

    if possible_add_conflict:
        print operator.name
    assert not possible_add_conflict, "Conflicting add effects?"

    assign_effect = []
    possible_assign_conflict = []
    assign_eff, possible_assign_conflict = \
        translate_assignment_effects(operator.assign_effects, dictionary, ranges, comp_axioms)
    
    if possible_assign_conflict:
        print operator.name
    assert not possible_assign_conflict, "Conflicting assign effects?"

    pre_post = []
    for var in effect:
        for (post, eff_condition_lists) in effect[var].iteritems():
            pre = condition.get(var, -1)
            if pre != -1:
                del condition[var]
            for eff_condition in eff_condition_lists:
                pre_post.append((var, pre, post, eff_condition))
    prevail = condition.items()

    assign_effects = []
    for var, ((op, valvar), eff_condition_lists) in assign_effect.iteritems():
        for eff_condition in eff_condition_lists:
            sas_effect = sas_tasks.SASAssignmentEffect(var, op, valvar, 
                                                       eff_condition, False)
            assign_effects.append(sas_effect)
    for var in assign_effect:
        for ((op, valvar), eff_condition_lists) in assign_effect[var].iteritems():
            for eff_condition in eff_condition_lists:
                sas_effect = sas_tasks.SASAssignmentEffect(var, op, valvar, 
                                                       eff_condition, True)
                assign_effects[time].append(sas_effect)

    return sas_tasks.SASOperator(operator.name, prevail, pre_post, assign_effects)


def translate_temporal_strips_operator(operator, dictionary, ranges, comp_axioms):
    # NOTE: This function does not really deal with the intricacies of properly
    # encoding delete effects for grouped propositions in the presence of
    # conditional effects. It should work ok but will bail out in more
    # complicated cases even though a conflict does not necessarily exist.

    duration = translate_operator_duration(operator.duration, dictionary)
    condition = translate_strips_conditions(operator.conditions, 
                                dictionary, ranges, comp_axioms, True)
    #Commented by R.Huang
    if condition is None:
        print "condition is None!!!!!!"
        return None

    effect = []
    possible_add_conflict = False
    for time in range(2):
        eff, poss_conflict = translate_add_effects(operator.add_effects[time], 
                                          dictionary, ranges, comp_axioms, True)
        translate_del_effects(operator.del_effects[time], dictionary, ranges, 
                              eff, condition, comp_axioms, True, time)
        effect.append(eff)
        possible_add_conflict |= poss_conflict

    if possible_add_conflict:
        print operator.name
    assert not possible_add_conflict

    assign_effect = []
    possible_assign_conflict = False
    for time in range(2):
        eff, conflict = translate_assignment_effects(operator.assign_effects[time], 
                                                     dictionary, ranges, comp_axioms, True)
        assign_effect.append(eff)
        possible_assign_conflict |= conflict
    
    if possible_assign_conflict:
        print operator.name
    assert not possible_assign_conflict

    pre_post = [[],[]]
    for time in range(2):
        cond_time = time*2 # start -> start condition, end -> end_condition
        for var in effect[time]:
            for (post, eff_condition_lists) in effect[time][var].iteritems():
                pre = condition[cond_time].get(var, -1)
                if pre != -1:
                    del condition[cond_time][var]
                for eff_condition in eff_condition_lists:
                    pre_post[time].append((var, pre, post, eff_condition))
    prevail = [cond.items() for cond in condition]

    assign_effects = [[],[]]
    for time in range(2):
        for var in assign_effect[time]:
            for ((op, valvar), eff_condition_lists) \
                in assign_effect[time][var].iteritems():
                for eff_condition in eff_condition_lists:
                    sas_effect = sas_tasks.SASAssignmentEffect(var, op, valvar, 
                                                           eff_condition, True)
                    assign_effects[time].append(sas_effect)

    sasoper = sas_tasks.SASTemporalOperator(operator.name, duration, 
                prevail, pre_post, assign_effects)
    sasoper.simple_duration = operator.simple_duration
    return sasoper
    
def translate_strips_axiom(axiom, dictionary, ranges, comp_axioms):
    condition = translate_strips_conditions(axiom.condition, dictionary, ranges, comp_axioms)
    if condition is None:
        return None
    if axiom.effect.negated:
        [(var, _)] = dictionary[axiom.effect.positive()]
        effect = (var, ranges[var] - 1)
    else:
        [effect] = dictionary[axiom.effect]
    return sas_tasks.SASAxiom(condition.items(), effect)

def translate_numeric_axiom(axiom, dictionary):
    effect = dictionary.get(axiom.effect)[0][0]
    op = axiom.op
    parts = []
    for part in axiom.parts:
        if isinstance(part, pddl.PrimitiveNumericExpression):
            parts.append(dictionary.get(part)[0][0])
        else: # part is PropositionalNumericAxiom
            parts.append(dictionary.get(part.effect)[0][0])
    return sas_tasks.SASNumericAxiom(op, parts, effect)

def translate_strips_operators(actions, strips_to_sas, ranges, comp_axioms):
    result = []
    actions.sort(lambda x,y: cmp(x.name,y.name))
    for action in actions:
        sas_op = translate_strips_operator(action, strips_to_sas, ranges, comp_axioms)
        if sas_op:
            result.append(sas_op)
    return result


#R.Huang
#Maintains 3 lists of transitions;
class TransBasedOperator():
    def __init__(self):
        self.duration = None
        self.overall  = set()   #(int,int,int), (int,int,int),... 
        self.start    = set()   #(int,int,int), (int,int,int),...
        self.end      = set()   #(int,int,int), (int,int,int),...
        self.name     = ""
    def output(self):
        print self.name, "[", self.duration,"]"
        print "S:",
        for item in self.start: print item,
        print 
        print "O:",
        for item in self.overall: print item,
        print 
        print "E:",
        for item in self.end: print item,
        print 
        
        
        
def convert_as_transitions(condition, aEffect, dEffect, s2s, ranges):
    
    
    for item in dEffect:
        item = item[1]
        flag = False
        for item2 in condition:
            if item2 == item:
                flag = True
                break
        assert flag
    
    rst = []
    for item in dEffect:
        item = item[1]
        mvar = s2s[item][0]
        for item2 in aEffect:
            mvar2 = s2s[item2[1]][0]
            if mvar[0] == mvar2[0]:
                trans = (mvar[0],mvar[1],mvar2[1])
                rst.append(trans)
        
        flag = False
        for item2 in dEffect:
            mvar2 = s2s[item2[1]][0]
            if mvar[0] == mvar2[0]:
                flag = True
        if not flag:
            rst.append( mvar )
        
    return rst
    
    
def translate_temporal_strips_operators(actions, strips_to_sas, ranges, comp_axioms):
    result = []
    actions.sort(lambda x,y: cmp(x.name,y.name))
    for action in actions:
        sas_op = translate_temporal_strips_operator(action, strips_to_sas, ranges, comp_axioms)
        if sas_op:
            result.append(sas_op)
        else:
            oper = TransBasedOperator()
            oper.name = action.name
            oper.duration = action.simple_duration
            oper.temp_negate_overall = []
            if str(action.name).startswith("(serve"):
                #R.Huang
                print "Hacking the serve action, where the parse keeps failing."
                
                for item in action.conditions[1]:
                    #Overall transitions
                    if not item.negated: 
                        trans = (strips_to_sas[item][0][0],strips_to_sas[item][0][1],strips_to_sas[item][0][1])
                        oper.overall.add(trans)
                    else:
                        pass
                        """
                        pitem = item.negate()
                        assert len(strips_to_sas[pitem]) == 1
                        mvar =  strips_to_sas[pitem][0]
                        if ranges[mvar[0]] == 2:
                            #Trivial case
                            if mvar[1] == 1:  mvar = (mvar[0],0)
                            else:  mvar =  (mvar[0],1)
                        else:
                            #Search for an corresponding add_effect in action.start
                            flag = False
                            for item2 in action.add_effects[0]:
                                item2 = item2[1]
                                if item2.negated: continue
                                mvar_s = strips_to_sas[item2][0]
                                if mvar_s[0] == mvar[0]:
                                    mvar = mvar_s
                                    flag = True
                                    break
                            assert flag 
                        oper.overall.append(mvar)
                        """
                        
                    #Start and End time transitions
                    cond = action.conditions
                    aEffects = action.add_effects
                    dEffects = action.del_effects
                    oper.start = convert_as_transitions(cond[0], aEffects[0], dEffects[0], strips_to_sas, ranges)
                    oper.end    = convert_as_transitions(cond[2], aEffects[1], dEffects[1], strips_to_sas, ranges)
                
                result.append(oper)
    return result

def translate_strips_axioms(axioms, strips_to_sas, ranges, comp_axioms):
    result = []
    axioms.sort(lambda x,y: cmp(x.name,y.name))
    for axiom in axioms:
        sas_axiom = translate_strips_axiom(axiom, strips_to_sas, ranges, comp_axioms)
        if sas_axiom:
            result.append(sas_axiom)
    return result

def translate_task(strips_to_sas, ranges, init, goals, actions, 
                   durative_actions, axioms, num_axioms):
    axioms, axiom_init, axiom_layer_dict = axiom_rules.handle_axioms(
      actions, durative_actions, axioms, goals)

    init = init + axiom_init
    
    num_axioms_by_layer, max_num_layer, const_num_axioms = \
        numeric_axiom_rules.handle_axioms(num_axioms)

    comp_axioms = []
    goal_pairs = translate_strips_conditions(goals, strips_to_sas, ranges, comp_axioms).items()
    goal = sas_tasks.SASGoal(goal_pairs)
    
    operators = translate_strips_operators(actions, strips_to_sas, ranges, comp_axioms)
    temp_operators = translate_temporal_strips_operators(durative_actions, 
                                                         strips_to_sas, ranges, comp_axioms)
        
    axioms = translate_strips_axioms(axioms, strips_to_sas, ranges, comp_axioms)
    sas_num_axioms = [translate_numeric_axiom(axiom,strips_to_sas) 
                        for axiom in num_axioms if axiom not in const_num_axioms]

    axiom_layers = [-1] * len(ranges)
    
    ## each numeric axiom gets its own layer (a wish of a colleague for 
    ## knowledge compilation or search. If you use only the translator,
    ## you can change this)
    num_axiom_layer = 0
    for layer in num_axioms_by_layer:
        num_axioms_by_layer[layer].sort(lambda x,y: cmp(x.name,y.name))
        for axiom in num_axioms_by_layer[layer]:
            [(var,val)] = strips_to_sas[axiom.effect]
            if layer == -1:
                axiom_layers[var] = -1
            else:
                axiom_layers[var] = num_axiom_layer
                num_axiom_layer += 1
    for axiom in comp_axioms:
        axiom_layers[axiom.effect] = num_axiom_layer
    for atom, layer in axiom_layer_dict.iteritems():
        assert layer >= 0
        [(var, val)] = strips_to_sas[atom]
        axiom_layers[var] = layer + num_axiom_layer + 1
    variables = sas_tasks.SASVariables(ranges, axiom_layers)

    init_values = [rang - 1 for rang in ranges]
    # Closed World Assumption: Initialize to "range - 1" == Nothing.
    for fact in init:
        if isinstance(fact,pddl.Atom):
            pairs = strips_to_sas.get(fact, [])  # empty for static init facts
            for var, val in pairs:
                assert init_values[var] == ranges[var] - 1, "Inconsistent init facts!"
                init_values[var] = val
        else: # isinstance(fact,pddl.FunctionAssignment)
            pairs = strips_to_sas.get(fact.fluent,[]) #empty for constant functions 
            for (var,dummy) in pairs:
                val = fact.expression.value
                assert init_values[var] == ranges[var] - 1, "Inconsistent init facts!"
                init_values[var]=val
    for axiom in const_num_axioms:
        var = strips_to_sas.get(axiom.effect)[0][0]
        val = axiom.parts[0].value
        init_values[var]=val
    init = sas_tasks.SASInit(init_values)

    return sas_tasks.SASTask(variables, init, goal, operators, 
                             temp_operators, axioms, sas_num_axioms, comp_axioms)

def unsolvable_sas_task(msg):
    print "%s! Generating unsolvable task..." % msg
    variables = sas_tasks.SASVariables([2], [-1])
    init = sas_tasks.SASInit([0])
    goal = sas_tasks.SASGoal([(0, 1)])
    operators = []
    temp_operators = []
    axioms = []
    num_axioms = []
    comp_axioms = []
    return sas_tasks.SASTask(variables, init, goal, operators,
            temp_operators, axioms, num_axioms, comp_axioms)


def refine_matchlift_helper(lst):
  """
  print "begin",
  for f in lst: print f,
  print
  """ 
  m = {}
  for fact in lst:
    name = str(fact)
    s = name.split("(")[1].split(",")[0]
    if s not in m:
      m[s] = [fact]
    else:
      m[s].append(fact)
  rst = []
  for k in m.keys():
    rst.append(m[k])
  """
  print "end", 
  for f in rst:
    for i in f: print i, 
    print ";;;;",
  print 
  """
  cnt = 0
  for item in rst: cnt+= len(item)
  assert cnt == len(lst)
  return rst
  
  
def refine_matchlift(groups):
  rst = []
  
  inroom_lst = []
  onfloor_lst = []
  inlift_lst = []
  liftonF_lst = []
  candidates = set()
  
  for group in groups:
    if len(group) > 1:
      rst.append(group)
    else:
      fact = group[0]
      name = str(fact)
      #print name
      #print name[5:].split("(")
      predicate = name[5:].split("(")[0]
      #print "PPPPPPPPPPPP", predicate
      if predicate == "inroom":
        inroom_lst.append(fact)
      elif predicate == "onfloor":
        onfloor_lst.append(fact)
      elif predicate == "inlift":
        inlift_lst.append(fact)
      elif predicate == "liftonfloor":
        liftonF_lst.append(fact)
      else:
        rst.append(group)
  rst.extend(refine_matchlift_helper(inroom_lst))
  rst.extend(refine_matchlift_helper(onfloor_lst))
  rst.extend(refine_matchlift_helper(inlift_lst))
  rst.extend(refine_matchlift_helper(liftonF_lst))
  
  #sys.exit()
  return rst
  
def pddl_to_sas(task):
    print "Instantiating..."
    (relaxed_reachable, atoms, num_fluents, actions, 
        durative_actions, axioms, num_axioms) = instantiate.explore(task)

    if not relaxed_reachable:
        return unsolvable_sas_task("No relaxed solution")

    num_axioms = list(num_axioms)
    num_axioms.sort(lambda x,y: cmp(x.name,y.name))

    # HACK! Goals should be treated differently (see TODO file).
    # Update: This is now done during normalization. The assertions
    # are only left here to be on the safe side. Can be removed eventually

    if isinstance(task.goal, pddl.Conjunction):
        goal_list = task.goal.parts
    else:
        goal_list = [task.goal]
    for item in goal_list:
        assert isinstance(item, pddl.Literal)

    groups, mutex_groups, translation_key = fact_groups.compute_groups(
        task, atoms, return_mutex_groups=WRITE_ALL_MUTEXES,
        partial_encoding=USE_PARTIAL_ENCODING)
    
    # START OF MERGING HACK !!!!
    """
    print fact_groups.__class__.__name__
    print len(groups),len(mutex_groups), "CCCCCCCCCCCCCCCc"
    for item in groups:
      print "G:",
      for i in item:
        print i,
      print 
    print "ZZZZZZZZZZZZZZZZZZZZ"
    for item in mutex_groups:
      print "G:",
      for i in item:
        print i,
      print
    """
    if task.domain_name == "matchlift":
      print "Hacking!"
      groups = refine_matchlift(groups)
    # END OF MERGING HACK !!!!
          
    print "Building STRIPS to SAS dictionary..."
    ranges, strips_to_sas = strips_to_sas_dictionary(groups, num_axioms, num_fluents)
    print "Translating task..."
    sas_task = translate_task(strips_to_sas, ranges, task.init, goal_list,
                              actions, durative_actions, axioms, num_axioms)

    mutex_key = build_mutex_key(strips_to_sas, mutex_groups)

#    try:
#        simplify.filter_unreachable_propositions(
#            sas_task, mutex_key, translation_key)
#    except simplify.Impossible:
#        return unsolvable_sas_task("Simplified to trivially false goal")

    write_translation_key(strips_to_sas)
    sas_task.strips_to_sas = strips_to_sas
    if WRITE_ALL_MUTEXES:
        write_mutex_key(mutex_key)
    return sas_task

def build_mutex_key(strips_to_sas, groups):
    group_keys = []
    for group in groups:
        group_key = []
        for fact in group:
            if strips_to_sas.get(fact):
                for var, val in strips_to_sas[fact]:
                    group_key.append((var, val, str(fact)))
            else:
                print "not in strips_to_sas, left out:", fact
        group_keys.append(group_key)
    return group_keys

def write_translation_key(strips_to_sas):
    var_file = file("variables.groups", "w")
    vars = dict()
    for exp,[(var, val)] in strips_to_sas.iteritems():
        vars.setdefault(var, []).append((val, exp))
    for var in range(len(vars)):
        print >> var_file, "var%d" % var
        vals = sorted(vars[var]) 
        for (val, exp) in vals:
            print >> var_file, "   %d: %s" % (val, exp)

def write_mutex_key(mutex_key):
    invariants_file = file("all.groups", "w")
    print >> invariants_file, "begin_groups"
    print >> invariants_file, len(mutex_key)
    for group in mutex_key:
        #print map(str, group)
        no_facts = len(group)
        print >> invariants_file, "group"
        print >> invariants_file, no_facts
        for var, val, fact in group:
            #print fact
            assert str(fact).startswith("Atom ")
            predicate = str(fact)[5:].split("(")[0]
            #print predicate
            rest = str(fact).split("(")[1]
            rest = rest.strip(")").strip()
            if not rest == "":
                #print "there are args" , rest
                args = rest.split(",")
            else:
                args = []
            print_line = "%d %d %s %d " % (var, val, predicate, len(args))
            for arg in args:
                print_line += str(arg).strip() + " "
            #print fact
            #print print_line
            print >> invariants_file, print_line
    print >> invariants_file, "end_groups"
    invariants_file.close()


if __name__ == "__main__":
    import pddl
    print "Parsing..."
    task = pddl.open()

    sas_task = pddl_to_sas(task)
    print "Writing output..."
    sas_task.output(file("output.sas", "w"))
    print "Done!"
