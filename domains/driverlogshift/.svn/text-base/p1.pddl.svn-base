(define (problem DLOG-2-2-2)
     (:domain driverlogshift)
     (:objects
          driver1 driver2 - driver
          truck1 truck2  - truck
          package1 package2 package3  - obj
          s0 s1 s2 s3  pa   - location)
     (:init
          (at driver2 pa)
          (rested driver2)
		  (at driver1 pa)
		  (rested driver1)
		  
          (at truck1 s0)
          (empty truck1)
          (at truck2 s2)
          (empty truck2)
          
          (at package1 s3)
          (at package2 s2)
		  (at package3 s1)
          
          (path s3 pa)
          (path pa s3)
          (path s0 s1)
		  (path s1 s0)
		  (path s1 s2)
		  (path s2 s1)
		  (path s2 s3)
		  (path s3 s2)
		  
          (link s0 s1)
          (link s1 s0)
          (link s2 s1)
          (link s1 s2)
          (link s2 s3)
          (link s3 s2)
          )
     (:goal (and
          (at package1 s2)
          (at package2 s1)
		  (at package3 s0)
		  ))
     (:metric minimize (total-time)))
