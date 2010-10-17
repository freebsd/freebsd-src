 BSPEC 80
 TETRA 2 % Decent section length name (in 32-bit words).  However...
 BYTE  "aaaa"
 BYTE  0x98,"aaa" # A LOP_QUOTEd part here.  And also...
 ESPEC % Everything ends here.  The next thing is a LOP_LOC for .data, or
       % an ending LOP-something, hence a non-LOP_QUOTE in the section flags.

 .data
 TETRA 0x112233
