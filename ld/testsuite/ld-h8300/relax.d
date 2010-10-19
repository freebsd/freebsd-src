# name: H8300 Relaxation Test
# ld: --relax
# objdump: -d

# Based on the test case reported by Kazu Hirata:
# http://sources.redhat.com/ml/binutils/2002-11/msg00301.html

.*:     file format .*-h8300

Disassembly of section .text:

00000100 <_start>:
 100:	0d 00.*mov.w	r0,r0
 102:	47 02.*beq	.+2 \(0x106\)
 104:	55 02.*bsr	.+2 \(0x108\)

00000106 <.L1>:
 106:	54 70.*rts	

00000108 <_bar>:
 108:	54 70.*rts	
