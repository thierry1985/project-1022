ulimit -t 1800 



i=1

while [ $i -le 12 ]
do


echo "solving problem #$i/12"

#./satplan  -problem "/home/cic/rh11/domains/SATELLITE/STRIPS/P0$i.PDDL"  -cgproblem "/home/cic/rh11/domains/SATELLITE/STRIPS/P0$i.PDDL" -domain "/home/cic/rh11/domains/SATELLITE/STRIPS/DOMAIN.PDDL" -cgdomain "/home/cic/rh11/domains/SATELLITE/STRIPS/DOMAIN.PDDL"  -londexm 1 >sattlite_result$i

crikey2.sh  domain.pddl p$i.pddl  p$i.pddl.soln >crikey2_p$i.rslt


i=`expr $i + 1`
done
