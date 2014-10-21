#! /usr/bin/env python

import random

PROBLEM_NAME = "P3"
FS = {}
routing = []
OBJ_computers = ["comp1","comp2","comp3","comp4"]
OBJ_files = ["A","B","C","D"]


for ob in OBJ_computers:
  for obj in OBJ_computers:
    if ob != obj:
      routing.append( (ob,obj))
for file in OBJ_files:
  FS[file] = random.randint( 4, 8 )

print '(define (problem %s)' % PROBLEM_NAME
print "(:domain P2P)"
print "(:objects"
print "       ",
for obj in OBJ_computers:
  print obj,
print " - computer"
print "       ",
for obj in OBJ_files:
  print obj,
print " - file "

print ")"
print ""
print ""

print "(:init"
for file in OBJ_files:
  print '  (saved %s %s)' % ( OBJ_computers[0], file)
for comp in OBJ_computers:
  print "  (free ", comp , " )"
for r in routing:
  print '  (routed %s %s)' % ( r[0], r[1])
for r in FS.keys():
  print '  (= (file-size %s) %s)' %( r , FS[r])
print ")"


#Goals:
print "(:goal "
print	"   (and"
for file in OBJ_files:
  for comp in OBJ_computers:
    print '     (saved %s %s)' % ( comp, file )
print "	   )"
print ")"
  
  
print "(:metric minimize (total-time))"

print ")"
