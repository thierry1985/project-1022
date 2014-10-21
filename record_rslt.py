#!/home/research/huangr/bin/python
import re,os
import signal
import resource
import sys
import sqlite3


def format(val):
  return float(int(val*1000))/1000


def pro(name):

  f = open(name,"r")
  var = 0
  clause = 0
  all_time = 0
  all_sat_time = 0 
  max_time = 0
  max_mem = 0
  level = 0
  while True:
    l = f.readline()
    if l == None or l=="":
      break
    
    l = l.strip()
    items = l.split(" ")
    if "Time Spent :" in l:
      all_time = float(l.split(":")[1].strip())
    #if "@SAT Time:" in l:
    #  all_sat_time = float(l.split(":")[1])
    if "Var:" in l :
      var =  int( l.split(":")[1])
    if "Clause:" in l: 
      clause = int(l.split(":")[1])
    if l[0:2] == "c " and "seconds" in l and "MB" in l and "recycled" in l:
      all_sat_time += float( l.split(" ")[1] )
      p1 = l.split(",")[1].strip().split(" ")[0]
      #print l, ":ZZZZ", p1
      mem = float(p1)
      if mem > max_mem:  max_mem = mem
    
    if "Make Span:" in l:
      level = int(l.split(":")[1])
  return [name, format(all_time), format(all_sat_time), int(max_mem), var, clause, level ]



if __name__ == "__main__":
  domain_name = sys.argv[1]
  print "domain name: ", domain_name
  
  result = {}
  for f in os.listdir("./"):
    if f.split("-")[0] == domain_name :
      t = f.split("-")[1].strip()
      result[t] =  pro(f)
  for i in range(50):
    rst = result.get(str(i))
    if rst:
      for i in rst:
        print i,"\t   ",
      print 

