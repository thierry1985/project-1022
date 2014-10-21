;
;
;

(define (problem PROBLEM_X)

(:domain toy)


(:objects
	t1 - truck
	c1 c2 - city
	a1 - cargo
)


(:init
	(at t1 c1)
	(at a1 c1)
	(connect c1 c2)
	(connect c2 c1)
)

(:goal
	(and
		(in a1 t1)
	)
)


(:metric minimize (total-time))

)
