#objdump: -dr --prefix-addresses --show-raw-insn
#name: FPA memory insructions
#as: -mfpu=fpa10 -mcpu=arm7m

# Test FPA memory instructions
# This test should work for both big and little-endian assembly.

.*: *file format .*arm.*

Disassembly of section .text:
0+00 <[^>]*> ed900100 ?	ldfs	f0, \[r0\]
0+04 <[^>]*> ec300101 ?	ldfs	f0, \[r0\], #-4
0+08 <[^>]*> ed908100 ?	ldfd	f0, \[r0\]
0+0c <[^>]*> ec308101 ?	ldfd	f0, \[r0\], #-4
0+10 <[^>]*> edd00100 ?	ldfe	f0, \[r0\]
0+14 <[^>]*> ec700101 ?	ldfe	f0, \[r0\], #-4
0+18 <[^>]*> edd08100 ?	ldfp	f0, \[r0\]
0+1c <[^>]*> ec708101 ?	ldfp	f0, \[r0\], #-4
0+20 <[^>]*> ed800100 ?	stfs	f0, \[r0\]
0+24 <[^>]*> ec200101 ?	stfs	f0, \[r0\], #-4
0+28 <[^>]*> ed808100 ?	stfd	f0, \[r0\]
0+2c <[^>]*> ec208101 ?	stfd	f0, \[r0\], #-4
0+30 <[^>]*> edc00100 ?	stfe	f0, \[r0\]
0+34 <[^>]*> ec600101 ?	stfe	f0, \[r0\], #-4
0+38 <[^>]*> edc08100 ?	stfp	f0, \[r0\]
0+3c <[^>]*> ec608101 ?	stfp	f0, \[r0\], #-4
0+40 <[^>]*> ed900200 ?	lfm	f0, 4, \[r0\]
0+44 <[^>]*> ed900200 ?	lfm	f0, 4, \[r0\]
0+48 <[^>]*> ed10020c ?	lfm	f0, 4, \[r0, #-48\]
0+4c <[^>]*> ed800200 ?	sfm	f0, 4, \[r0\]
0+50 <[^>]*> ed00020c ?	sfm	f0, 4, \[r0, #-48\]
0+54 <[^>]*> ed800200 ?	sfm	f0, 4, \[r0\]
0+58 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+5c <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
