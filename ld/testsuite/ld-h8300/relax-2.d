# name: H8300 Relaxation Test 2
# ld: --relax -m h8300helf
# objdump: -d --no-show-raw-insn

.*:     file format .*-h8300

Disassembly of section .text:

00000100 <_start>:
 *100:	mov.b	@0x67:8,r0l
 *102:	mov.b	@0x4321:16,r0l
