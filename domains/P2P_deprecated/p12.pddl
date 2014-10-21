;number of nodes  6
;file rate  4
(define (problem P3)
(:domain P2P)
(:objects
        comp1 comp2 comp3 comp4 comp5 comp6  - computer
        file0A file0B file0C file0D file1A file1B file1C file1D file2A file2B file2C file2D file3A file3B file3C file3D file4A file4B file4C file4D file5A file5B file5C file5D  - file 
)


(:init
  (saved comp1 file0A)
  (saved comp1 file0B)
  (saved comp1 file0C)
  (saved comp1 file0D)
  (saved comp2 file1A)
  (saved comp2 file1B)
  (saved comp2 file1C)
  (saved comp2 file1D)
  (saved comp3 file2A)
  (saved comp3 file2B)
  (saved comp3 file2C)
  (saved comp3 file2D)
  (saved comp4 file3A)
  (saved comp4 file3B)
  (saved comp4 file3C)
  (saved comp4 file3D)
  (saved comp5 file4A)
  (saved comp5 file4B)
  (saved comp5 file4C)
  (saved comp5 file4D)
  (saved comp6 file5A)
  (saved comp6 file5B)
  (saved comp6 file5C)
  (saved comp6 file5D)
  (free  comp1  )
  (free  comp2  )
  (free  comp3  )
  (free  comp4  )
  (free  comp5  )
  (free  comp6  )
  (routed comp1 comp2)
  (routed comp1 comp3)
  (routed comp1 comp4)
  (routed comp1 comp5)
  (routed comp1 comp6)
  (routed comp2 comp1)
  (routed comp2 comp3)
  (routed comp2 comp4)
  (routed comp2 comp5)
  (routed comp2 comp6)
  (routed comp3 comp1)
  (routed comp3 comp2)
  (routed comp3 comp4)
  (routed comp3 comp5)
  (routed comp3 comp6)
  (routed comp4 comp1)
  (routed comp4 comp2)
  (routed comp4 comp3)
  (routed comp4 comp5)
  (routed comp4 comp6)
  (routed comp5 comp1)
  (routed comp5 comp2)
  (routed comp5 comp3)
  (routed comp5 comp4)
  (routed comp5 comp6)
  (routed comp6 comp1)
  (routed comp6 comp2)
  (routed comp6 comp3)
  (routed comp6 comp4)
  (routed comp6 comp5)
  (= (file-size file3A) 4)
  (= (file-size file3C) 5)
  (= (file-size file3B) 7)
  (= (file-size file5C) 7)
  (= (file-size file3D) 7)
  (= (file-size file5A) 5)
  (= (file-size file1C) 7)
  (= (file-size file1B) 5)
  (= (file-size file1A) 6)
  (= (file-size file1D) 4)
  (= (file-size file5D) 7)
  (= (file-size file4A) 5)
  (= (file-size file4B) 5)
  (= (file-size file4C) 5)
  (= (file-size file4D) 7)
  (= (file-size file2B) 4)
  (= (file-size file2C) 4)
  (= (file-size file2A) 6)
  (= (file-size file2D) 7)
  (= (file-size file5B) 4)
  (= (file-size file0D) 4)
  (= (file-size file0A) 5)
  (= (file-size file0B) 4)
  (= (file-size file0C) 4)
)
(:goal 
   (and
     (saved comp1 file0A)
     (saved comp2 file0A)
     (saved comp3 file0A)
     (saved comp4 file0A)
     (saved comp5 file0A)
     (saved comp6 file0A)
     (saved comp1 file0B)
     (saved comp2 file0B)
     (saved comp3 file0B)
     (saved comp4 file0B)
     (saved comp5 file0B)
     (saved comp6 file0B)
     (saved comp1 file0C)
     (saved comp2 file0C)
     (saved comp3 file0C)
     (saved comp4 file0C)
     (saved comp5 file0C)
     (saved comp6 file0C)
     (saved comp1 file0D)
     (saved comp2 file0D)
     (saved comp3 file0D)
     (saved comp4 file0D)
     (saved comp5 file0D)
     (saved comp6 file0D)
     (saved comp1 file1A)
     (saved comp2 file1A)
     (saved comp3 file1A)
     (saved comp4 file1A)
     (saved comp5 file1A)
     (saved comp6 file1A)
     (saved comp1 file1B)
     (saved comp2 file1B)
     (saved comp3 file1B)
     (saved comp4 file1B)
     (saved comp5 file1B)
     (saved comp6 file1B)
     (saved comp1 file1C)
     (saved comp2 file1C)
     (saved comp3 file1C)
     (saved comp4 file1C)
     (saved comp5 file1C)
     (saved comp6 file1C)
     (saved comp1 file1D)
     (saved comp2 file1D)
     (saved comp3 file1D)
     (saved comp4 file1D)
     (saved comp5 file1D)
     (saved comp6 file1D)
     (saved comp1 file2A)
     (saved comp2 file2A)
     (saved comp3 file2A)
     (saved comp4 file2A)
     (saved comp5 file2A)
     (saved comp6 file2A)
     (saved comp1 file2B)
     (saved comp2 file2B)
     (saved comp3 file2B)
     (saved comp4 file2B)
     (saved comp5 file2B)
     (saved comp6 file2B)
     (saved comp1 file2C)
     (saved comp2 file2C)
     (saved comp3 file2C)
     (saved comp4 file2C)
     (saved comp5 file2C)
     (saved comp6 file2C)
     (saved comp1 file2D)
     (saved comp2 file2D)
     (saved comp3 file2D)
     (saved comp4 file2D)
     (saved comp5 file2D)
     (saved comp6 file2D)
     (saved comp1 file3A)
     (saved comp2 file3A)
     (saved comp3 file3A)
     (saved comp4 file3A)
     (saved comp5 file3A)
     (saved comp6 file3A)
     (saved comp1 file3B)
     (saved comp2 file3B)
     (saved comp3 file3B)
     (saved comp4 file3B)
     (saved comp5 file3B)
     (saved comp6 file3B)
     (saved comp1 file3C)
     (saved comp2 file3C)
     (saved comp3 file3C)
     (saved comp4 file3C)
     (saved comp5 file3C)
     (saved comp6 file3C)
     (saved comp1 file3D)
     (saved comp2 file3D)
     (saved comp3 file3D)
     (saved comp4 file3D)
     (saved comp5 file3D)
     (saved comp6 file3D)
     (saved comp1 file4A)
     (saved comp2 file4A)
     (saved comp3 file4A)
     (saved comp4 file4A)
     (saved comp5 file4A)
     (saved comp6 file4A)
     (saved comp1 file4B)
     (saved comp2 file4B)
     (saved comp3 file4B)
     (saved comp4 file4B)
     (saved comp5 file4B)
     (saved comp6 file4B)
     (saved comp1 file4C)
     (saved comp2 file4C)
     (saved comp3 file4C)
     (saved comp4 file4C)
     (saved comp5 file4C)
     (saved comp6 file4C)
     (saved comp1 file4D)
     (saved comp2 file4D)
     (saved comp3 file4D)
     (saved comp4 file4D)
     (saved comp5 file4D)
     (saved comp6 file4D)
     (saved comp1 file5A)
     (saved comp2 file5A)
     (saved comp3 file5A)
     (saved comp4 file5A)
     (saved comp5 file5A)
     (saved comp6 file5A)
     (saved comp1 file5B)
     (saved comp2 file5B)
     (saved comp3 file5B)
     (saved comp4 file5B)
     (saved comp5 file5B)
     (saved comp6 file5B)
     (saved comp1 file5C)
     (saved comp2 file5C)
     (saved comp3 file5C)
     (saved comp4 file5C)
     (saved comp5 file5C)
     (saved comp6 file5C)
     (saved comp1 file5D)
     (saved comp2 file5D)
     (saved comp3 file5D)
     (saved comp4 file5D)
     (saved comp5 file5D)
     (saved comp6 file5D)
	   )
)
(:metric minimize (total-time))
)