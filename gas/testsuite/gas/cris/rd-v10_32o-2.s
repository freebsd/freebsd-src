a:
 ba b1
 .space 32767-4
b1:
 ba b2
 ba b2
 .space 127*2-2
b2:
 .space 128*2
 ba b2
 ba b2
b3:
 .space 32764
 ba b3
b4:
 setf
aa:
 beq bb1
 .space 32767-4
bb1:
 beq bb2
 beq bb2
 .space 127*2-2
bb2:
 .space 128*2
 beq bb2
 beq bb2
bb3:
 .space 32764
 beq bb3
bb4:
