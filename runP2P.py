#!/usr/bin/env python
import sys
import os
from benchmarks import * 


def curr_jqueue_size():
    
    os.system("qstat > OWN_TEMP_FILE ")
    f = open("OWN_TEMP_FILE","r")
    lines = f.readlines()
    os.remove("./OWN_TEMP_FILE") 
    return len(lines)




#os.system("rm CNF_* -fr")
PROGRESS_FNAME = "progress.running"

#airport    elevator   mprime       pathways-noneg        pipesworld-tankage  scanalyzer  transport
#blocks     freecell   mystery      pegsol                psr-large           sokoban     trucks
#depot      gripper    openstacks   philosophers          rovers              storage     woodworking
#driverlog  logistics  parcprinter  pipesworld-notankage  satellite           tpp         zenotravel

#batch1 = ["elevator","mystery","openstacks","parcprinter","pathways","pegsol","scan","sokoban","storage","tpp"]
#batch2 = ["airport","pipesworld-notankage","pipesworld-tankage","psr","transport","trucks","zenotravel","rovers"]
#batch3 = ["blocks","depot","driverlog","freecell","gripper","logistics","mprime","philosophers","satellite","zenotravel"]
#batch4 = ["woodworking"]


for i in range(1,17):
    #./src/TSS.py /project/cic3/huangr/TemporalSASE/domain.pddl /project/cic3/huangr/TemporalSASE/domains/P2P/p1.pddl
    print "Running", i
    run_str = " ./translate/TSS.py /project/cic3/huangr/TemporalSASE/domains/p2p-domain.pddl /project/cic3/huangr/TemporalSASE/domains/P2P/p%d.pddl > P2P-%d-result" % (i,i)
    os.system( run_str )
