# Use of .greg; non-mmixal syntax though somewhat corresponding to greg1.
# Note that use-before-definition is allowed.
 .text

 .greg A0,0
 .greg B1,1
 .greg C3,D4
 .greg D5,
 .greg ,E6+24 % Somewhat unusable, but hey...
 .greg F7,.+8
 .greg ,.+8
 .greg G8,0xf7
 .greg H9,.+8

D4:
 set $123,456
E6:
 set $234,7899
 swym 2,3,4
Main:
 swym 1,2,3
