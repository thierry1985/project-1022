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

dd = re.compile(r"[\S]+?soln")
for f in os.listdir("./"):
	if f == "readsoln.py":
		continue;
	m = dd.match(f)
	if m:
		file_list.append( f )
file_list.sort(compare_file)

print "ID\t time \t\t Makespan"
index = 1
for f in file_list:
	fname = re.compile(r"[\S]+?([0-9]+)[\S]+")
	mn = fname.match(f)
	if not mn:
		continue;
	#while (int(mn.group(1))!=index):
	#	print index
	#	index = index + 1
	#; Time 18.65
	file = open(f,"r")
	dt = re.compile(r";Span\s(.+)")	
	m = dt.match(file.readline())
	makespan = -1
	#; MakeSpan 5
	dt = re.compile(r";Time[\s]+([\S]+)")
	str = file.readline()

	n = dt.match(str)
	if n != None:
		makespan = float(n.group(1))
		print mn.group(1), "\t", m.group(1),"\t\t",n.group(1), "\t",f
	index = index + 1
