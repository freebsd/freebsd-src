a:
 ba b1
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
 .space 32768
 ba b3
 ba b3
b4:
