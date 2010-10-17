; Test error cases for constant ranges.

;  { dg-do assemble { target cris-*-* } }

 .set two701867, 2701867
 .set mtwo701867, -2701867
 .set const_int_32, 0x1b94452b
 .set const_int_m32, -3513208907
 .set three2767, 32767

 .text
 .syntax no_register_prefix
start:
 moveq 32,r0 ; { dg-error "Immediate value not in 6 bit range: 32" }
 moveq 63,r0 ; { dg-error "Immediate value not in 6 bit range: 63" }
 moveq 0x20,r0 ; { dg-error "Immediate value not in 6 bit range: 32" }
 moveq 0x3f,r0 ; { dg-error "Immediate value not in 6 bit range: 63" }
 moveq -33,r0 ; { dg-error "Immediate value not in 6 bit range: -33" }
 addq 64,r0 ; { dg-error "Immediate value not in 6 bit unsigned range: 64" }
 addq -1,r0 ; { dg-error "Immediate value not in 6 bit unsigned range: -1" }
 subq 64,r0 ; { dg-error "Immediate value not in 6 bit unsigned range: 64" }
 subq -1,r0 ; { dg-error "Immediate value not in 6 bit unsigned range: -1" }
 break 16 ; { dg-error "Immediate value not in 4 bit unsigned range: 16" }
 movs.b 256,r0 ; { dg-error "Immediate value not in 8 bit range: 256" }
 movs.b 255,r0 ; { dg-error "Immediate value not in 8 bit range: 255" "" { xfail *-*-* } }
 movs.b -129,r0 ; { dg-error "Immediate value not in 8 bit range: -129" }
 movs.b 128,r0 ; { dg-error "Immediate value not in 8 bit range: 128" "" { xfail *-*-* } }
 movs.b -32769,r0 ; { dg-error "Immediate value not in (8|16) bit range: -32769" }
 movs.b 0xffffffff,r0 ; { dg-error "Immediate value not in (8|16) bit range: (4294967295|-1)" "" { xfail *-*-* } }

 movs.w 32768,r0 ; { dg-error "Immediate value not in 16 bit range: 32768" "" { xfail *-*-* } }
 movs.w 0x8000,r0 ; { dg-error "Immediate value not in 16 bit range: 32768" "" { xfail *-*-* } }
 movs.w 65535,r0 ; { dg-error "Immediate value not in 16 bit range: 65535" "" { xfail *-*-* } }
 movs.w 0xffff,r0 ; { dg-error "Immediate value not in 16 bit range: 65535" "" { xfail *-*-* } }
 movs.w -32769,r0 ; { dg-error "Immediate value not in 16 bit range: -32769" }
 movs.w 65536,r0 ; { dg-error "Immediate value not in 16 bit range: 65536" }
 movs.w -32769,r0 ; { dg-error "Immediate value not in 16 bit range: -32769" }
 movs.w 0xffffffff,r0 ; { dg-error "Immediate value not in 16 bit range: (4294967295|-1)" "" { xfail *-*-* } }

 movu.b 256,r0 ; { dg-error "Immediate value not in 8 bit range: 256" }
 movu.b 0x100,r0 ; { dg-error "Immediate value not in 8 bit range: 256" }
 movu.b -1,r0 ; { dg-error "Immediate value not in 8 bit unsigned range: -1" "" { xfail *-*-* } }
 movu.b -127,r0 ; { dg-error "Immediate value not in 8 bit unsigned range: -127" "" { xfail *-*-* } }
 movu.b -129,r0 ; { dg-error "Immediate value not in 8 bit range: -129" }
 movu.b -128,r0 ; { dg-error "Immediate value not in 8 bit unsigned range: -128" "" { xfail *-*-* } }

 movu.w 65536,r0 ; { dg-error "Immediate value not in 16 bit range: 65536" }
 movu.w -32769,r0 ; { dg-error "Immediate value not in 16 bit range: -32769" }
 movu.w -1,r0 ; { dg-error "Immediate value not in 16 bit unsigned range: -1" "" { xfail *-*-* } }
 movu.w 0xffffffff,r0 ; { dg-error "Immediate value not in 16 bit (unsigned )?range: (4294967295|-1)" "" { xfail *-*-* } }

 add.b -129,r5 ; { dg-error "Immediate value not in 8 bit range: -129" }
 add.b -255,r5 ; { dg-error "Immediate value not in 8 bit range: -255" }
 add.b 256,r5 ; { dg-error "Immediate value not in 8 bit range: 256" }
 add.b -8856,r5 ; { dg-error "Immediate value not in 8 bit range: -8856" }
 add.b 8856,r5 ; { dg-error "Immediate value not in 8 bit range: 8856" }

 add.w two701867,r13 ; { dg-error "Immediate value not in 16 bit range: 2701867" }
 add.w mtwo701867,r13 ; { dg-error "Immediate value not in 16 bit range: -2701867" }

 add.w 2781868,r13 ; { dg-error "Immediate value not in 16 bit range: 2781868" }
 add.w -2701867,r13 ; { dg-error "Immediate value not in 16 bit range: -2701867" }

 add.w 0x9ec0ceac,r13 ; { dg-error "Immediate value not in 16 bit range: -1631531348" }
 add.w -0x7ec0cead,r13 ; { dg-error "Immediate value not in 16 bit range: -2126565037" }

 add.w const_int_m32,r13 ; { dg-error "Immediate value not in 16 bit range: 781758389" }
 add.w const_int_32,r13 ; { dg-error "Immediate value not in 16 bit range: 462701867" }
 add.w -(three2767+2),r5 ; { dg-error "Immediate value not in 16 bit range: -32769" }
