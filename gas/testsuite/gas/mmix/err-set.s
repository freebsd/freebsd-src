% { dg-do assemble { target mmix-*-* } }
Main SET $45,23
 SET $57,$67 % Valid, Z is 0.
 SET $78,X % Valid, Z is 0.
 SET $7,Y % { dg-error "invalid operands.*value of 967 too large" "" }
X IS $31
Y IS 967
