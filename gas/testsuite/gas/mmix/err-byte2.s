% { dg-do assemble { target mmix-*-* } }

m1 IS -1
zero IS 0
zero2 IS 0
1H IS 42
2H IS 5
Main SWYM 0,0,0
 BYTE 0
 BYTE -1 % { dg-error "BYTE expression not in the range 0..255" "" }
 BYTE m1 % { dg-error "BYTE expression not in the range 0..255" "" }
 BYTE zero2
 BYTE 1B+2B+55
 BYTE zero+m1 % { dg-error "BYTE expression not in the range 0..255" "" }
 BYTE 255
 BYTE 256 % { dg-error "BYTE expression not in the range 0..255" "" }
 BYTE unk+1 % { dg-error "BYTE expression not a pure number" "" }
