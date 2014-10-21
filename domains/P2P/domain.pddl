;
; Objects:
;     File,Computer,
;
; Action:
;     download : always constant 2
;     establish-service   : depends on file size;

(define (domain P2P)
(:requirements :typing :fluents :durative-actions)


(:types file computer - object )


(:predicates
	(saved ?p - computer ?f - file)
  (routed ?x ?y - computer )  
  (serving ?x - computer ?f - file)
  (free ?x - computer)
)


(:functions (file-size ?f - file) )

(:durative-action serve
 :parameters
	( ?c - computer ?f - file )
 :duration (= ?duration (file-size ?f) )
 :condition
	( and 
        (at start (free ?c) )
				(over all (not(free ?c )))
        (at start (saved ?c ?f))
        (over all (serving ?c ?f) )
	)
 :effect(and 
 					(at start  (not (free ?c )) )
					(at end    (free ?c ) )
					(at start  (serving ?c ?f) )
					(at end    (not (serving ?c ?f) ) )
	)
)


(:durative-action download
 :parameters
	( ?self ?server - computer ?f - file )
 :duration (= ?duration 2)
 :condition
	( and 
				(at start (serving ?server ?f ))
				(over all (serving ?server ?f ))
				(at end (serving ?server ?f ))
				(over all (routed ?server ?self) )
	)
 :effect
	( and 
				(at end    (saved ?self ?f ))
  )
)

)


