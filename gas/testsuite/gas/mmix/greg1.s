# Use of GREG with mmixal syntax.
D4 SET $123,456
E6 SET $234,7899

A0 GREG 0
B1 GREG 1
C3 GREG D4
D5 GREG
   GREG E6+24
F7 GREG @
   GREG @  % Equivalent to F7, unless -no-merge-gregs.
G8 GREG #F7
H9 GREG @  % Equivalent to F7, unless -no-merge-gregs.

 SWYM 2,3,4
Main SWYM 1,2,3
