#! /usr/bin/env python

import random

PROBLEM_NAME = "ELEC2_2"

#Configurations
num_floor = 2
num_fuse = 8
num_elec = 3
num_room = [ 3, 3]   #number of rooms for each floors
has_strict_setting = True

PROBLEM_NAME = "Elec" + str(num_elec) + "_Fuse" + str(num_fuse) + "Floor" + str(num_floor)

#******END ADJUSTABLE PARAMETERS***********

max_fuse_per_room = num_elec * 2




########  Determines the number of fuses in each room;

fuse_info = []
available_fuses = num_fuse 
num_match = 0

if has_strict_setting:
  rand_floor = random.randint( 0, num_floor-1 )
  rand_room = random.randint(0,num_room[rand_floor]-1 )  
  
  available_fuses -= max_fuse_per_room
  allocate_fuse = max_fuse_per_room  
  for i in range(allocate_fuse):
    fuse_info.append( (rand_floor, rand_room) )
  num_match += 1

while available_fuses!=0:
  rand_floor = random.randint( 0, num_floor-1 )
  rand_room = random.randint(0,num_room[rand_floor]-1 )  
  if (rand_floor,rand_room) in fuse_info:
    continue
  
  allocate_fuse = random.randint( 1, max_fuse_per_room -1  ) 
  if allocate_fuse > available_fuses:
    allocate_fuse = available_fuses
  
  for i in range(allocate_fuse):
    fuse_info.append( (rand_floor, rand_room) )
  available_fuses -= allocate_fuse
  num_match += 1
  
num_lift = 2

#Objects
lifts = []
floors = []
rooms = [ [] for i in range(num_floor)]
elecs = []
fuses = []
matches = []

#Auto-Generate Objects
for i in range(1,num_lift+1):
  lifts.append( "lift" + str(i) )
for i in range(1,num_floor+1):
  floors.append( "floor" + str(i) )
for i in range(1,num_elec+1):
  elecs.append( "elec" + str(i) )
for i in range(num_fuse):
    fuses.append( "fuse" + str(i+1) )
for i in range(0,num_floor):
  for j in range(0,num_room[i-1]):
    room_id = chr( ord('a') + j)
    rooms[i].append( "room" + str(i+1) + room_id )
for i in range(num_match):
  matches.append( "match" + str(i+1))
    
print '(define (problem %s)' % PROBLEM_NAME
print "(:domain matchlift)"
print "(:objects"
print "       ",
for obj in floors:
  print obj,
print " -  floor "

print "       ",
for obj in matches:
  print obj,
print " - match "

print "       ",
for obj in lifts:
  print obj,
print " -  lift "

print "       ",
for obj in fuses:
  print obj,
print " -  fuse "

print "       ",
for obj in elecs:
  print obj,
print " -  electrician "

print "       ",
for i in range(num_floor):
  for room in rooms[i]:
    print room,
print " -  room "


print ")"
print ""
print ""

print "(:init"
for match in matches:
  print '  (unused %s)' % match 
for elec in elecs:
  print '  (handfree %s)' % elec
  print '  (onfloor %s %s)' % (elec, floors[0])
for i in range(num_floor):
  for room in rooms[i]:
    print '  (roomonfloor %s %s)' % (room,floors[i])
for lift in lifts:
  print '  (liftonfloor %s %s)' % (lift , floors[0])
for i,fuse in enumerate(fuses):
  room = rooms[fuse_info[i][0]][fuse_info[i][1]]
  print '  (fuseinroom %s %s)' % (fuse,room)
for index in range(num_floor-1):
  print '  (connectedfloors %s %s)' % (floors[index],floors[index+1])
  print '  (connectedfloors %s %s)' % (floors[index+1],floors[index])
print ")"


#Goals:
print "   "
print "(:goal (and"
for fuse in fuses:
  print '    (mended %s)' % fuse
print "))"
  
  
print "(:metric minimize (total-time))"

print ")"
