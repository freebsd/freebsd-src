# name: H8300 Relaxation Test 6
# source: relax-6.s
# ld: --relax -m h8300s
# objdump: -d --no-show-raw-insn

.*:     file format .*-h8300
Disassembly of section .text:

00000100 <_start>:
 100:	mov.b	r2l,@0xbd:8
 102:	rts	
