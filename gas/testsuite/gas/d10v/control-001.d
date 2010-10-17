#objdump: -Dr
#source: control-001.s

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	69 00 56 00 	mvfc	r0, psw	->	mvtc	r0, psw
   4:	69 01 56 20 	mvfc	r0, bpsw	->	mvtc	r0, bpsw
   8:	69 02 56 40 	mvfc	r0, pc	->	mvtc	r0, pc
   c:	69 03 56 60 	mvfc	r0, bpc	->	mvtc	r0, bpc
  10:	69 07 56 e0 	mvfc	r0, rpt_c	->	mvtc	r0, rpt_c
  14:	69 08 57 00 	mvfc	r0, rpt_s	->	mvtc	r0, rpt_s
  18:	69 09 57 20 	mvfc	r0, rpt_e	->	mvtc	r0, rpt_e
  1c:	69 0a 57 40 	mvfc	r0, mod_s	->	mvtc	r0, mod_s
  20:	69 0b 57 60 	mvfc	r0, mod_e	->	mvtc	r0, mod_e
  24:	69 0e 57 c0 	mvfc	r0, iba	->	mvtc	r0, iba
