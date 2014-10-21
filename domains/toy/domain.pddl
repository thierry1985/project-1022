;
;
;
;
;

(define (domain toy)
(:requirements :adl)


(:types object
	truck cargo - object 
	city - object)

(:predicates
	(at ?o - object ?c - city)
	(in ?a - cargo ?t - truck)
	(connect ?c1 ?c2 - city)
)


(:durative-action move
 :parameters
	( ?c1 - city ?c2 - city ?t - truck)
 :duration (= ?duration 2 )	
 :condition
	( and (at start (at ?t ?c1))
	      (over all (connect ?c1 ?c2))
	      )
 :effect
	( and 
		(at end (at ?t ?c2) )
		(at start (not (at ?t ?c1)))
	
	)
)



(:durative-action load
    :parameters
	( ?c - city ?t - truck ?a - cargo)
    :duration (= ?duration 2 )	
    :condition
	( and 
	      (over all (at ?t ?c) )
	      (at start (at ?a ?c) )
	)
    :effect
	( and 
	      (at end (in ?a ?t)  )
	      (at start (not (at ?a ?c)))
	)
)

(:durative-action unload
    :parameters
	( ?c - city ?t - truck ?a - cargo )
    :duration (= ?duration 2 )
    :condition
	( and 
	      (over all (at ?t ?c) )
	      (at start (in ?a ?t)) 
	)
    :effect
	( and 
	     (at end (at ?a ?c) )
	     (at start (not (in ?a ?t) )  )
	)
)

)
