 BSPEC 80
 TETRA 2 % Decent section length name (in 32-bit words).
 BYTE  "aaaaaaaa"
 TETRA 0x11 % Flags.
 TETRA 0,12 % Decent section length.  However...
 TETRA 0 % Things end stops after the high part of the VMA.
 ESPEC
 .data
 TETRA 0x112233
