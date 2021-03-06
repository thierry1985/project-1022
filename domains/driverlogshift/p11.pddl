(define (problem DLOG-2-2-2)
     (:domain driverlogshift)
     (:objects
          driver1 driver2 - driver
          truck1 truck2 - truck
          package1 package2 package3 package4 package5 package6 package7 package8 - obj
          s0 s1 s2 s3 s4 s5 s6 s7 s8 p1-0 p1-2 - location)
     (:init
          (at driver1 s2)
          (rested driver1)
          (at driver2 s2)
          (rested driver2)
          (at truck1 s0)
          (empty truck1)
          (at truck2 s0)
          (empty truck2)
          (at package1 s0)
          (at package2 s0)
          (at package3 s0)
          (at package4 s0)
          (at package5 s0)
          (at package6 s0)
          (at package7 s0)
          (at package8 s0)
          (path s1 p1-0)
          (path p1-0 s1)
          (path s0 p1-0)
          (path p1-0 s0)
          (path s1 p1-2)
          (path p1-2 s1)
          (path s2 p1-2)
          (path p1-2 s2)
          (link s0 s1)
          (link s1 s0)
          (link s0 s2)
          (link s2 s0)
          (link s2 s1)
          
          (link s3 s2)
          (link s2 s3)
          
          (link s3 s4)
          (link s4 s3)
          (link s4 s5)
          (link s5 s4)
          (link s5 s6)
          (link s6 s5)
          (link s6 s7)
          (link s7 s6)          
          (link s7 s8)
          (link s8 s7)
          )
     (:goal (and
          (at driver1 s1)
          (rested driver1)
          (at truck1 s0)
          (at package1 s1)
          (at package2 s2)
          (at package3 s3)
          (at package4 s4)
          (at package5 s5)
          (at package6 s6)
          (at package7 s7)
          (at package8 s8)

          ))
     (:metric minimize (total-time)))
