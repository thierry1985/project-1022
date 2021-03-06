(define (problem ELEC2_2)
(:domain matchlift)
(:objects
        floor1 floor2 floor3 floor4  -  floor 
        match1 match2 match3 match4  - match 
        lift1 lift2  -  lift 
        fuse1 fuse2 fuse3 fuse4  -  fuse 
        elec1 elec2  -  electrician 
        room1a room1b room1c room1d room2a room2b room2c room2d room3a room3b room3c room3d room4a room4b room4c room4d  -  room 
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
  (roomonfloor room1a floor1)
  (roomonfloor room1b floor1)
  (roomonfloor room1c floor1)
  (roomonfloor room1d floor1)
  (roomonfloor room2a floor2)
  (roomonfloor room2b floor2)
  (roomonfloor room2c floor2)
  (roomonfloor room2d floor2)
  (roomonfloor room3a floor3)
  (roomonfloor room3b floor3)
  (roomonfloor room3c floor3)
  (roomonfloor room3d floor3)
  (roomonfloor room4a floor4)
  (roomonfloor room4b floor4)
  (roomonfloor room4c floor4)
  (roomonfloor room4d floor4)
  (liftonfloor lift1 floor1)
  (liftonfloor lift2 floor1)
  (fuseinroom fuse1 room3b)
  (fuseinroom fuse2 room4b)
  (fuseinroom fuse3 room1d)
  (fuseinroom fuse4 room3d)
  (connectedfloors floor1 floor2)
  (connectedfloors floor2 floor1)
  (connectedfloors floor2 floor3)
  (connectedfloors floor3 floor2)
  (connectedfloors floor3 floor4)
  (connectedfloors floor4 floor3)
)
   
(:goal (and
    (mended fuse1)
    (mended fuse2)
    (mended fuse3)
    (mended fuse4)
))
(:metric minimize (total-time))
)
