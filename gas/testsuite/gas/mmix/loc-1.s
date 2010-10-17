# Check that we don't get anything strange from a single LOC to data and a
# LOC back (with an offset).
Main SWYM 0,0,0
m2 LOC Data_Segment
 TETRA 4
 LOC m2+24
 SWYM 1,2,3
