a:
 beq b1
 bne b1
 .space 32767-4
b1:
 bhs b2
 bhi b2
 .space 127*2-2
b2:
 .space 128*2
 bcs b2
 blo b2
b3:
 .space 32768
 bls b3
 bsb b3
b4:


  