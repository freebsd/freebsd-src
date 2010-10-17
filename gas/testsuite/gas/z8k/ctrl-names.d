#as:
#objdump: -d
#name: jmp cc

.*: +file format coff-z8k

Disassembly of section \.text:

00000000 <\.text>:
   0:	7d02           	ldctl	r0,fcw
   2:	7d0a           	ldctl	fcw,r0
   4:	7d03           	ldctl	r0,refresh
   6:	7d0b           	ldctl	refresh,r0
   8:	7d04           	ldctl	r0,psapseg
   a:	7d0c           	ldctl	psapseg,r0
   c:	7d05           	ldctl	r0,psapoff
   e:	7d0d           	ldctl	psapoff,r0
  10:	7d05           	ldctl	r0,psapoff
  12:	7d0d           	ldctl	psapoff,r0
  14:	7d06           	ldctl	r0,nspseg
  16:	7d0e           	ldctl	nspseg,r0
  18:	7d07           	ldctl	r0,nspoff
  1a:	7d0f           	ldctl	nspoff,r0
  1c:	7d07           	ldctl	r0,nspoff
  1e:	7d0f           	ldctl	nspoff,r0
  20:	7d02           	ldctl	r0,fcw
  22:	7d0a           	ldctl	fcw,r0
  24:	7d03           	ldctl	r0,refresh
  26:	7d0b           	ldctl	refresh,r0
  28:	7d04           	ldctl	r0,psapseg
  2a:	7d0c           	ldctl	psapseg,r0
  2c:	7d05           	ldctl	r0,psapoff
  2e:	7d0d           	ldctl	psapoff,r0
  30:	7d05           	ldctl	r0,psapoff
  32:	7d0d           	ldctl	psapoff,r0
  34:	7d06           	ldctl	r0,nspseg
  36:	7d0e           	ldctl	nspseg,r0
  38:	7d07           	ldctl	r0,nspoff
  3a:	7d0f           	ldctl	nspoff,r0
  3c:	7d07           	ldctl	r0,nspoff
  3e:	7d0f           	ldctl	nspoff,r0
