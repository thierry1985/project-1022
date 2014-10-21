;
;This problem has all the computers fully connected;
;in the initial  state, computer1 has all the files 

(define (problem P2)
(:domain P2P)
(:objects
        comp1 comp2 comp3 comp4 comp5 comp6  - computer
        A B C D E F G  - file 
)


(:init
  (saved comp1 A)
  (saved comp1 B)
  (saved comp1 C)
  (saved comp1 D)
  (saved comp1 E)
  (saved comp1 F)
  (saved comp1 G)
  (free  comp1  )
  (free  comp2  )
  (free  comp3  )
  (free  comp4  )
  (free  comp5  )
  (free  comp6  )
  (routed comp1 comp2)
  (routed comp1 comp3)
  (routed comp1 comp4)
  (routed comp1 comp5)
  (routed comp1 comp6)
  (routed comp2 comp1)
  (routed comp2 comp3)
  (routed comp2 comp4)
  (routed comp2 comp5)
  (routed comp2 comp6)
  (routed comp3 comp1)
  (routed comp3 comp2)
  (routed comp3 comp4)
  (routed comp3 comp5)
  (routed comp3 comp6)
  (routed comp4 comp1)
  (routed comp4 comp2)
  (routed comp4 comp3)
  (routed comp4 comp5)
  (routed comp4 comp6)
  (routed comp5 comp1)
  (routed comp5 comp2)
  (routed comp5 comp3)
  (routed comp5 comp4)
  (routed comp5 comp6)
  (routed comp6 comp1)
  (routed comp6 comp2)
  (routed comp6 comp3)
  (routed comp6 comp4)
  (routed comp6 comp5)
  (= (file-size A) 4)
  (= (file-size C) 9)
  (= (file-size B) 4)
  (= (file-size E) 7)
  (= (file-size D) 4)
  (= (file-size G) 7)
  (= (file-size F) 5)
)
(:goal 
   (and
     (saved comp1 A)
     (saved comp2 A)
     (saved comp3 A)
     (saved comp4 A)
     (saved comp5 A)
     (saved comp6 A)
     (saved comp1 B)
     (saved comp2 B)
     (saved comp3 B)
     (saved comp4 B)
     (saved comp5 B)
     (saved comp6 B)
     (saved comp1 C)
     (saved comp2 C)
     (saved comp3 C)
     (saved comp4 C)
     (saved comp5 C)
     (saved comp6 C)
     (saved comp1 D)
     (saved comp2 D)
     (saved comp3 D)
     (saved comp4 D)
     (saved comp5 D)
     (saved comp6 D)
     (saved comp1 E)
     (saved comp2 E)
     (saved comp3 E)
     (saved comp4 E)
     (saved comp5 E)
     (saved comp6 E)
     (saved comp1 F)
     (saved comp2 F)
     (saved comp3 F)
     (saved comp4 F)
     (saved comp5 F)
     (saved comp6 F)
     (saved comp1 G)
     (saved comp2 G)
     (saved comp3 G)
     (saved comp4 G)
     (saved comp5 G)
     (saved comp6 G)
	   )
)
(:metric minimize (total-time))
)