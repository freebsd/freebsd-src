 .text
y:
 .comm c1,4,1
 .comm c2,4,1
 .comm c3,4,1
 move.d c1,$r10
 move.d c2:GOT,$r10
 move.d c3:PLT,$r10
