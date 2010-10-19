#source: abs32-1.s
#as: --em=criself --march=v32
#objdump: -dr

# Check that jump-type instructions to absolute addresses
# assemble and disassemble correctly for v32 given "old-style"
# mnemonics.

.*:     file format elf32.*-cris

Disassembly of section \.text:

00000000 <locsym2-0x2>:
   0:	b005                	nop 

00000002 <locsym2>:
   2:	b005                	nop 
   4:	bf0d 0000 0000      	jump 0 <locsym2-0x2>
			6: R_CRIS_32	\.text\+0x2
   a:	bf0d 0000 0000      	jump 0 <locsym2-0x2>
			c: R_CRIS_32	locsym2
  10:	bf0d 0000 0000      	jump 0 <locsym2-0x2>
			12: R_CRIS_32	locsym3
  16:	bf0d 0000 0000      	jump 0 <locsym2-0x2>
			18: R_CRIS_32	\.text\+0x74
  1c:	bf0d 0000 0000      	jump 0 <locsym2-0x2>
			1e: R_CRIS_32	extsym
  22:	bfbd 0000 0000      	jsr 0 <locsym2-0x2>
			24: R_CRIS_32	\.text\+0x2
  28:	bfbd 0000 0000      	jsr 0 <locsym2-0x2>
			2a: R_CRIS_32	locsym2
  2e:	bfbd 0000 0000      	jsr 0 <locsym2-0x2>
			30: R_CRIS_32	locsym3
  34:	bfbd 0000 0000      	jsr 0 <locsym2-0x2>
			36: R_CRIS_32	\.text\+0x74
  3a:	bfbd 0000 0000      	jsr 0 <locsym2-0x2>
			3c: R_CRIS_32	extsym
  40:	3fbf 0000 0000      	jsrc 0 <locsym2-0x2>
			42: R_CRIS_32	\.text\+0x2
  46:	0000                	bcc \.
  48:	0000                	bcc \.
  4a:	3fbf 0000 0000      	jsrc 0 <locsym2-0x2>
			4c: R_CRIS_32	locsym2
  50:	0000                	bcc \.
  52:	0000                	bcc \.
  54:	3fbf 0000 0000      	jsrc 0 <locsym2-0x2>
			56: R_CRIS_32	locsym3
  5a:	0000                	bcc \.
  5c:	0000                	bcc \.
  5e:	3fbf 0000 0000      	jsrc 0 <locsym2-0x2>
			60: R_CRIS_32	\.text\+0x74
  64:	0000                	bcc \.
  66:	0000                	bcc \.
  68:	3fbf 0000 0000      	jsrc 0 <locsym2-0x2>
			6a: R_CRIS_32	extsym
  6e:	0000                	bcc \.
  70:	0000                	bcc \.
  72:	b005                	nop 

00000074 <locsym3>:
  74:	b005                	nop 
	\.\.\.
