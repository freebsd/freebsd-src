 .text
y:
 .weak uw1
 .weak uw2
 .weak uw3
 move.d uw1,$r10
 move.d uw2:GOT,$r10
 move.d uw3:PLT,$r10
