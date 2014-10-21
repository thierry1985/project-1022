(define (problem Elec3_Fuse8Floor2)
(:domain matchlift)
(:objects
        floor1 floor2  -  floor 
        match1 match2 match3 match4  - match 
        lift1 lift2  -  lift 
        fuse1 fuse2 fuse3 fuse4 fuse5 fuse6 fuse7 fuse8  -  fuse 
        elec1 elec2 elec3  -  electrician 
        room1a room1b room1c room2a room2b room2c  -  room 
)


(:init
  (unused match1)
  (unused match2)
  (unused match3)
  (unused match4)
  (handfree elec1)
  (onfloor elec1 floor1)
  (handfree elec2)
  (onfloor elec2 floor1)
  (handfree elec3)
  (onfloor elec3 floor1)
  (roomonfloor room1a floor1)
  (roomonfloor room1b floor1)
  (roomonfloor room1c floor1)
  (roomonfloor room2a floor2)
  (roomonfloor room2b floor2)
  (roomonfloor room2c floor2)
  (liftonfloor lift1 floor1)
  (liftonfloor lift2 floor1)
  (fuseinroom fuse1 room1c)
  (fuseinroom fuse2 room1c)
  (fuseinroom fuse3 room2b)
  (fuseinroom fuse4 room2b)
  (fuseinroom fuse5 room2a)
  (fuseinroom fuse6 room2a)
  (fuseinroom fuse7 room2a)
  (fuseinroom fuse8 room2c)
  (connectedfloors floor1 floor2)
  (connectedfloors floor2 floor1)
)
   
(:goal (and
    (mended fuse1)
    (mended fuse2)
    (mended fuse3)
    (mended fuse4)
    (mended fuse5)
    (mended fuse6)
    (mended fuse7)
    (mended fuse8)
))
(:metric minimize (total-time))
)
