# Check that a little bit of LOC:ing back and forward between code and
# data section doesn't hurt.
 LOC #20 << 56
a TETRA 4*4

 LOC #200
 SWYM 911
Main SWYM 9,1,1

 LOC a+4
 TETRA 8*8

 LOC Main+4
 SWYM 101
