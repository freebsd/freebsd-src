#source: abs32-1.s
#as: --em=criself
#objdump: -dr

# Check that jump-type instructions to absolute addresses
# assemble and disassemble correctly.

.*:     file format .*-cris

Disassembly of section \.text:

00000000 <locsym2-0x2>:
   0:	0f05                	nop 

00000002 <locsym2>:
   2:	0f05                	nop 
   4:	3f0d 0200 0000      	jump 2 <locsym2>
			6: R_CRIS_32	\.text\+0x2
   a:	3f0d 0000 0000      	jump 0 <locsym2-0x2>
			c: R_CRIS_32	locsym2
  10:	3f0d 0000 0000      	jump 0 <locsym2-0x2>
			12: R_CRIS_32	locsym3
  16:	3f0d 7400 0000      	jump 74 <locsym3>
			18: R_CRIS_32	\.text\+0x74
  1c:	3f0d 0000 0000      	jump 0 <locsym2-0x2>
			1e: R_CRIS_32	extsym
  22:	3fbd 0200 0000      	jsr 2 <locsym2>
			24: R_CRIS_32	\.text\+0x2
  28:	3fbd 0000 0000      	jsr 0 <locsym2-0x2>
			2a: R_CRIS_32	locsym2
  2e:	3fbd 0000 0000      	jsr 0 <locsym2-0x2>
			30: R_CRIS_32	locsym3
  34:	3fbd 7400 0000      	jsr 74 <locsym3>
			36: R_CRIS_32	\.text\+0x74
  3a:	3fbd 0000 0000      	jsr 0 <locsym2-0x2>
			3c: R_CRIS_32	extsym
  40:	3f3d 0200 0000      	jsrc 2 <locsym2>
			42: R_CRIS_32	\.text\+0x2
  46:	0000                	bcc \.\+2
  48:	0000                	bcc \.\+2
  4a:	3f3d 0000 0000      	jsrc 0 <locsym2-0x2>
			4c: R_CRIS_32	locsym2
  50:	0000                	bcc \.\+2
  52:	0000                	bcc \.\+2
  54:	3f3d 0000 0000      	jsrc 0 <locsym2-0x2>
			56: R_CRIS_32	locsym3
  5a:	0000                	bcc \.\+2
  5c:	0000                	bcc \.\+2
  5e:	3f3d 7400 0000      	jsrc 74 <locsym3>
			60: R_CRIS_32	\.text\+0x74
  64:	0000                	bcc \.\+2
  66:	0000                	bcc \.\+2
  68:	3f3d 0000 0000      	jsrc 0 <locsym2-0x2>
			6a: R_CRIS_32	extsym
  6e:	0000                	bcc \.\+2
  70:	0000                	bcc \.\+2
  72:	0f05                	nop 

00000074 <locsym3>:
  74:	0f05                	nop 
	\.\.\.
