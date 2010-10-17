# Use of PREFIX; sanity check only.
 .text
a TETRA b
:c TETRA :d

 PREFIX pre
a TETRA b
:pre:c TETRA :pre:d

 PREFIX fix
a TETRA b
:pre:fix:c TETRA :pre:fix:d

 PREFIX :aprefix
a TETRA b
:aprefix:c TETRA :aprefix:d

 PREFIX :
a0 TETRA a
d TETRA c
