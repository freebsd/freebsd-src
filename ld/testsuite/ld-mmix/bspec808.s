 BSPEC 80
 TETRA 2 % Decent section length name (in 32-bit words).
 BYTE  "aaaaaaaa"
 TETRA 0x11 % Flags.
 TETRA 0xff00,0 % Indecent section length
 TETRA 0xff,0 % Decent vma.
 ESPEC
 .data
 TETRA 0x112233
