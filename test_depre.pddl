(define (problem ELEC2)
(:domain matchlift)
(:objects
        floor1 floor2 -  floor 
        match1  - match 
        lift1  -  lift 
        fuse1 -  fuse 
        elec1 -  electrician 
        room1a -  room 
)


(:init
  (unused match1)
  (handfree elec1)
  (onfloor elec1 floor1)
  (liftonfloor lift1 floor1)
  (fuseinroom fuse1 room1a)
  (connectedfloors floor1 floor2)
  (connectedfloors floor2 floor1)
)
   
(:goal (and
    (mended fuse1)
))
(:metric minimize (total-time))
)
