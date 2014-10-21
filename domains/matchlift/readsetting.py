#!/usr/bin/python
import re,os

def compare_file(x,y):
	dr = re.compile(r"[\S]+?([0-9]+)[\S]+")
	orderx = -1
	ordery = -1
	m = dr.match(x)
	if m:
		orderx = int(m.group(1))
	m = dr.match(y)
	if m:
		ordery = int(m.group(1))
	if orderx == -1 or ordery== -1:
		return 0
	if orderx>ordery:
		return 1
	elif orderx==ordery:
		return 0
	else:
		return -1

file_list = []

dd = re.compile(r"pe[\S]+?pddl$")
for f in os.listdir("./"):
	if f == "readsoln.py":
		continue;
	m = dd.match(f)
	if m:
		file_list.append( f )
file_list.sort(compare_file)

 
index = 1
for f in file_list:

  file = open(f,"r")
  for line in file.readlines():
    if "-" not in line:
      continue
    
    t = line.split("-")[1]
    t.strip()
    l = line.split("-")[0]
    l.strip()
    
    if "electrician" in t:
      print "electrician", l.count("elec"),
    elif "fuse" in t:
      print "fuse", l.count("fuse"),
    elif "match" in t:
      print "match", l.count("match"),
    elif "room" in t:
      print "room", l.count("room"),
    elif "floor" in t:
      print "floor", l.count("floor"),      
  print f