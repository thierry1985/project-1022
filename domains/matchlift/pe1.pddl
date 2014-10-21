(define (problem ELEC2)
(:domain matchlift)
(:objects
        floor1 floor2  -  floor 
        match1 match2 match3  - match 
        lift1 lift2  -  lift 
        fuse1 fuse2 fuse3  -  fuse 
        elec1 elec2  -  electrician 
        room1a room1b room2a room2b  -  room 
)


(:init
  (unused match1)
  (unused match2)
  (unused match3)
  (handfree elec1)
  (onfloor elec1 floor1)
  (handfree elec2)
  (onfloor elec2 floor1)
  (roomonfloor room1a floor1)
  (roomonfloor room1b floor1)
  (roomonfloor room2a floor2)
  (roomonfloor room2b floor2)
  (liftonfloor lift1 floor1)
  (liftonfloor lift2 floor1)
  (fuseinroom fuse1 room1a)
  (fuseinroom fuse2 room1a)
  (fuseinroom fuse3 room2b)
  (connectedfloors floor1 floor2)
  (connectedfloors floor2 floor1)
)
   
(:goal (and
    (mended fuse1)
    (mended fuse2)
    (mended fuse3)
))
(:metric minimize (total-time))
)
