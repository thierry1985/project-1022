;
; Computer A and B
; Two Files;

(define (problem p2p_simple)

(:domain P2P)


(:objects
	comp1 comp2 - computer
	A B - file
)


(:init
	(saved comp1 A)
	(saved comp1 B)
  (free comp1)
  (free comp2)
	(routed comp1 comp2)
	(routed comp2 comp1)
	
	(= (file-size A) 6)
	(= (file-size B) 4)
)

(:goal
	(and
    (saved comp2 A)
		 (saved comp2 B)
    (saved comp1 A)
    (saved comp1 B)
	)
)

(:metric minimize (total-time))

)
