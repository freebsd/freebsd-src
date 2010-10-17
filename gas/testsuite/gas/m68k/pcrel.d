#name: pcrel
#objdump: -drs -j .text --prefix-addresses

.*:     file format .*

Contents of section .text:
 0000 4e714e71 4cfa0300 fffa4cfa 0300fff4  NqNqL.....L.....
 0010 4cfb0300 08ee41fa ffea41fa ffe641fa  L.....A...A...A.
 0020 ff6241fb 08de41fb 08da41fb 08d641fb  .bA...A...A...A.
 0030 0920ffd2 41fb0920 ffcc41fb 0930ffff  . ..A.. ..A..0..
 0040 ffc641fb 0930ffff ffbe4e71 61ff0000  ..A..0....Nqa...
 0050 00566100 0050614c 614a4e71 41fa0046  .Va..PaLaJNqA..F
 0060 41fa0042 41fa00be 41fb083a 41fb0836  A..BA...A..:A..6
 0070 41fb0832 41fb0920 002e41fb 09200028  A..2A.. ..A.. .\(
 0080 41fb0930 00000022 41fb0930 0000001a  A..0..."A..0....
 0090 41fb0930 00000012 41fb0920 000a41fb  A..0....A.. ..A.
 00a0 08044e71 4e714e71 41fb0880 41fb0920  ..NqNqNqA...A.. 
 00b0 ff7f41fb 09208000 41fb0930 ffff7fff  ..A.. ..A..0....
 00c0 4e7141fb 087f41fb 09200080 41fb0920  NqA...A.. ..A.. 
 00d0 7fff41fb 09300000 80004e71 41fa8000  ..A..0....NqA...
 00e0 41fb0170 ffff7fff 4e7141fa 7fff41fb  A..p....NqA...A.
 00f0 01700000 80004e71 41fb0170 00000000  .p....NqA..p....
 0100 41fb0930 00000000 4e7141f9 00000000  A..0....NqA.....
 0110 4e710000 00000000                    Nq........      
Disassembly of section \.text:
0+0000 <.*> nop
0+0002 <lbl_b> nop
0+0004 <lbl_b\+(0x|)2> moveml %pc@\(0+0002 <lbl_b>\),%a0-%a1
0+000a <lbl_b\+(0x|)8> moveml %pc@\(0+0002 <lbl_b>\),%a0-%a1
0+0010 <lbl_b\+(0x|)e> moveml %pc@\(0+02 <lbl_b>,%d0:l\),%a0-%a1
0+0016 <lbl_b\+(0x|)14> lea %pc@\(0+0002 <lbl_b>\),%a0
0+001a <lbl_b\+(0x|)18> lea %pc@\(0+0002 <lbl_b>\),%a0
0+001e <lbl_b\+(0x|)1c> lea %pc@\(f+ff82 <.*>\),%a0
0+0022 <lbl_b\+(0x|)20> lea %pc@\(0+02 <lbl_b>,%d0:l\),%a0
0+0026 <lbl_b\+(0x|)24> lea %pc@\(0+02 <lbl_b>,%d0:l\),%a0
0+002a <lbl_b\+(0x|)28> lea %pc@\(0+02 <lbl_b>,%d0:l\),%a0
0+002e <lbl_b\+(0x|)2c> lea %pc@\(0+02 <lbl_b>,%d0:l\),%a0
0+0034 <lbl_b\+(0x|)32> lea %pc@\(0+02 <lbl_b>,%d0:l\),%a0
0+003a <lbl_b\+(0x|)38> lea %pc@\(0+02 <lbl_b>,%d0:l\),%a0
0+0042 <lbl_b\+(0x|)40> lea %pc@\(0+02 <lbl_b>,%d0:l\),%a0
0+004a <lbl_b\+(0x|)48> nop
0+004c <lbl_b\+(0x|)4a> bsrl 0+00a4 <lbl_a>
0+0052 <lbl_b\+(0x|)50> bsrw 0+00a4 <lbl_a>
0+0056 <lbl_b\+(0x|)54> bsrs 0+00a4 <lbl_a>
0+0058 <lbl_b\+(0x|)56> bsrs 0+00a4 <lbl_a>
0+005a <lbl_b\+(0x|)58> nop
0+005c <lbl_b\+(0x|)5a> lea %pc@\(0+00a4 <lbl_a>\),%a0
0+0060 <lbl_b\+(0x|)5e> lea %pc@\(0+00a4 <lbl_a>\),%a0
0+0064 <lbl_b\+(0x|)62> lea %pc@\(0+0124 <lbl_a\+0x80>\),%a0
0+0068 <lbl_b\+(0x|)66> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+006c <lbl_b\+(0x|)6a> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+0070 <lbl_b\+(0x|)6e> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+0074 <lbl_b\+(0x|)72> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+007a <lbl_b\+(0x|)78> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+0080 <lbl_b\+(0x|)7e> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+0088 <lbl_b\+(0x|)86> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+0090 <lbl_b\+(0x|)8e> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+0098 <lbl_b\+(0x|)96> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+009e <lbl_b\+(0x|)9c> lea %pc@\(0+00a4 <lbl_a>,%d0:l\),%a0
0+00a2 <lbl_b\+(0x|)a0> nop
0+00a4 <lbl_a> nop
0+00a6 <lbl_a\+(0x|)2> nop
0+00a8 <lbl_a\+(0x|)4> lea %pc@\(0+002a <lbl_b\+0x28>,%d0:l\),%a0
0+00ac <lbl_a\+(0x|)8> lea %pc@\(0+002d <lbl_b\+0x2b>,%d0:l\),%a0
0+00b2 <lbl_a\+(0x|)e> lea %pc@\(f+80b4 <lbl_a\+0xf+8010>,%d0:l\),%a0
0+00b8 <lbl_a\+(0x|)14> lea %pc@\(f+80b9 <lbl_a\+0xf+8015>,%d0:l\),%a0
0+00c0 <lbl_a\+(0x|)1c> nop
0+00c2 <lbl_a\+(0x|)1e> lea %pc@\(0+0143 <lbl_a\+0x9f>,%d0:l\),%a0
0+00c6 <lbl_a\+(0x|)22> lea %pc@\(0+0148 <lbl_a\+0xa4>,%d0:l\),%a0
0+00cc <lbl_a\+(0x|)28> lea %pc@\(0+80cd <lbl_a\+0x8029>,%d0:l\),%a0
0+00d2 <lbl_a\+(0x|)2e> lea %pc@\(0+80d4 <lbl_a\+0x8030>,%d0:l\),%a0
0+00da <lbl_a\+(0x|)36> nop
0+00dc <lbl_a\+(0x|)38> lea %pc@\(f+80de <lbl_a\+0xf+803a>\),%a0
0+00e0 <lbl_a\+(0x|)3c> lea %pc@\(f+80e1 <lbl_a\+0xf+803d>\),%a0
0+00e8 <lbl_a\+(0x|)44> nop
0+00ea <lbl_a\+(0x|)46> lea %pc@\(0+80eb <lbl_a\+0x8047>\),%a0
0+00ee <lbl_a\+(0x|)4a> lea %pc@\(0+80f0 <lbl_a\+0x804c>\),%a0
0+00f6 <lbl_a\+(0x|)52> nop
0+00f8 <lbl_a\+(0x|)54> lea %pc@\(0+00fa <lbl_a\+0x56>\),%a0
			fc: R_68K_PC32	undef\+0x2
0+0100 <lbl_a\+(0x|)5c> lea %pc@\(0+0102 <lbl_a\+0x5e>,%d0:l\),%a0
			104: R_68K_PC32	undef\+0x2
0+0108 <lbl_a\+(0x|)64> nop
0+010a <lbl_a\+(0x|)66> lea 0+0000 <lbl_b\-0x2>,%a0
			10c: R_68K_32	undef
0+0110 <lbl_a\+(0x|)6c> nop
0+0112 <lbl_a\+(0x|)6e> orib #0,%d0
	\.\.\.
