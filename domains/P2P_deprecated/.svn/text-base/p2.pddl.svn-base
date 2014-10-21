(define (problem P2)
(:domain P2P)
(:objects
        comp1 comp2  - computer
        A B C  - file 
)


(:init
  (saved comp1 A)
  (saved comp1 B)
  (saved comp1 C)
  (free  comp1  )
  (free  comp2  )
  (routed comp1 comp2)
  (routed comp2 comp1)
  (= (file-size A) 4)
  (= (file-size C) 8)
  (= (file-size B) 9)
)
(:goal 
   (and
     ;(saved comp1 A)
     (saved comp2 A)
     ;(saved comp1 B)
     (saved comp2 B)
     ;(saved comp1 C)
     (saved comp2 C)
	   )
)
(:metric minimize (total-time))
)
