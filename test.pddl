;LINEAR TOPOLOGY SETTING
(define (problem P3)
(:domain P2P)
(:objects
        comp1 comp2 comp3  - computer
        file0  - file 
)


(:init
  (saved comp1 file0)
  (free  comp1)
  (free  comp2)
  (free  comp3)
  (routed comp1 comp2)
  (routed comp2 comp1)
  (routed comp2 comp3)
  (routed comp3 comp2)
  (= (file-size file0) 4)
)
(:goal 
   (and
     (saved comp1 file0)
     (saved comp2 file0)
     (saved comp3 file0)
     ;(saved comp1 file2)
     ;(saved comp2 file2)
     ;(saved comp3 file2)
	   )
)
(:metric minimize (total-time))
)
