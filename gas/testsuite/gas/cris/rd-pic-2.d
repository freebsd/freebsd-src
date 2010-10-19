#objdump: -dr
#as: --underscore --em=criself --pic

# Check that 16-bit PIC relocs aren't overflowing.
# PR gas/1049.

.*:     file format .*-cris

Disassembly of section \.text:
0+ <a>:
       0:	7f9c 0000           	movs\.w 0,r9
			2: R_CRIS_16_GOT	y
       4:	7f9c 0000           	movs\.w 0,r9
			6: R_CRIS_16_GOTPLT	z
	\.\.\.
0+10008 <y>:
   10008:	0f05                	nop 
0+1000a <z>:
	\.\.\.
