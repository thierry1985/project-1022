;
; Computer A and B
; Two Files;

(define (problem p2p_simple)

(:domain P2P)


(:objects
	comp1 comp2 comp3 - computer
	A B C - file
)


(:init
	(saved comp1 A)
	(saved comp1 B)
	(saved comp1 C)
  (free comp1)
  (free comp2)
  (free comp3)
	(routed comp1 comp2)
	(routed comp2 comp1)
	(routed comp2 comp3)
	(routed comp3 comp2)
	(routed comp3 comp1)
	(routed comp1 comp3)
	
	(= (file-size A) 4)
	(= (file-size B) 4)
	(= (file-size C) 4)
)

(:goal
	(and
    (saved comp2 A)
    (saved comp2 B)
    (saved comp1 A)
    (saved comp1 B)
    (saved comp1 C)
    (saved comp3 A)
    (saved comp3 B)
    (saved comp3 C)
    (saved comp2 C)
	)
)

(:metric minimize (total-time))

)
