# name: H8300 GCC section test case
# ld: --gc-sections  -m h8300helf
# objdump: -d --no-show-raw-insn
.*:     file format .*-h8300

Disassembly of section .text:

00000100 <_functionWeUse>:
 100:	mov.l	er6,@-er7
 104:	mov.l	er7,er6
 106:	subs	#4,er7
 108:	mov.w	r0,@\(0xfffe:16,er6\)
 10c:	mov.w	@\(0xfffe:16,er6\),r2
 110:	mov.w	r2,r0
 112:	adds	#4,er7
 114:	mov.l	@er7\+,er6
 118:	rts	

0000011a <_start>:
 11a:	mov.l	er6,@-er7
 11e:	mov.l	er7,er6
 120:	mov.w	#0x4b,r0
 124:	jsr	@0x100:24
 128:	mov.w	r0,r2
 12a:	mov.w	r2,r0
 12c:	mov.l	@er7\+,er6
 130:	rts	
