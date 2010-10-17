# name: H8300 Relaxation Test 4
# source: relax-4.s
# ld: --relax -m h8300s
# objdump: -d --no-show-raw-insn

.*:     file format .*-h8300

Disassembly of section .text:

00000100 <_start>:
 100:	f8 03             mov.b	#0x3,r0l
 102:	fa 05             mov.b	#0x5,r2l
 104:	7f ff 60 80       bset	r0l,@0xff:8
 108:	7f 00 60 a0       bset	r2l,@0x0:8
 10c:	7e ff 63 a0       btst	r2l,@0xff:8
 110:	7e 00 63 80       btst	r0l,@0x0:8
 114:	6a 18 00 00 70 50 bset	#0x5,@0x0:16
 11a:	6a 18 7f ff 70 50 bset	#0x5,@0x7fff:16
 120:	6a 18 80 00 70 50 bset	#0x5,@0x8000:16
 126:	6a 18 fe ff 70 50 bset	#0x5,@0xfeff:16
 12c:	7f 00 70 50       bset	#0x5,@0x0:8
 130:	7f ff 70 50       bset	#0x5,@0xff:8
 134:	6a 10 00 00 76 50 band	#0x5,@0x0:16
 13a:	6a 10 7f ff 76 50 band	#0x5,@0x7fff:16
 140:	6a 10 80 00 76 50 band	#0x5,@0x8000:16
 146:	6a 10 fe ff 76 50 band	#0x5,@0xfeff:16
 14c:	7e 00 76 50       band	#0x5,@0x0:8
 150:	7e ff 76 50       band	#0x5,@0xff:8
 154:	7f ff 60 a0       bset	r2l,@0xff:8
 158:	7f 00 60 80       bset	r0l,@0x0:8
 15c:	7e ff 63 80       btst	r0l,@0xff:8
 160:	7e 00 63 a0       btst	r2l,@0x0:8
 164:	6a 18 00 00 70 60 bset	#0x6,@0x0:16
 16a:	6a 18 7f ff 70 60 bset	#0x6,@0x7fff:16
 170:	6a 38 00 00 80 00 70 60 bset	#0x6,@0x8000:32
 178:	6a 38 00 00 ff 00 70 60 bset	#0x6,@0xff00:32
 180:	6a 38 00 ff ff 00 70 60 bset	#0x6,@0xffff00:32
 188:	6a 38 ff ff 7f ff 70 60 bset	#0x6,@0xffff7fff:32
 190:	6a 18 80 00 70 60 bset	#0x6,@0x8000:16
 196:	6a 18 fe ff 70 60 bset	#0x6,@0xfeff:16
 19c:	7f 00 70 60       bset	#0x6,@0x0:8
 1a0:	7f ff 70 60       bset	#0x6,@0xff:8
 1a4:	6a 10 00 00 76 60 band	#0x6,@0x0:16
 1aa:	6a 10 7f ff 76 60 band	#0x6,@0x7fff:16
 1b0:	6a 30 00 00 80 00 76 60 band	#0x6,@0x8000:32
 1b8:	6a 30 00 00 ff 00 76 60 band	#0x6,@0xff00:32
 1c0:	6a 30 00 ff ff 00 76 60 band	#0x6,@0xffff00:32
 1c8:	6a 30 ff ff 7f ff 76 60 band	#0x6,@0xffff7fff:32
 1d0:	6a 10 80 00 76 60 band	#0x6,@0x8000:16
 1d6:	6a 10 fe ff 76 60 band	#0x6,@0xfeff:16
 1dc:	7e 00 76 60       band	#0x6,@0x0:8
 1e0:	7e ff 76 60       band	#0x6,@0xff:8
