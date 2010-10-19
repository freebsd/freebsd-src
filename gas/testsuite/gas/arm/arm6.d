# name: ARM 6 instructions
# as: -mcpu=arm6
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <[^>]+> e10f8000 ?	mrs	r8, CPSR
0+04 <[^>]+> e14f2000 ?	mrs	r2, SPSR
0+08 <[^>]+> e129f001 ?	msr	CPSR_fc, r1
0+0c <[^>]+> 1328f20f ?	msrne	CPSR_f, #-268435456	; 0xf0000000
0+10 <[^>]+> e168f008 ?	msr	SPSR_f, r8
0+14 <[^>]+> e169f009 ?	msr	SPSR_fc, r9
0+18 <[^>]+> e10f8000 ?	mrs	r8, CPSR
0+1c <[^>]+> e14f2000 ?	mrs	r2, SPSR
0+20 <[^>]+> e129f001 ?	msr	CPSR_fc, r1
0+24 <[^>]+> 1328f20f ?	msrne	CPSR_f, #-268435456	; 0xf0000000
0+28 <[^>]+> e168f008 ?	msr	SPSR_f, r8
0+2c <[^>]+> e169f009 ?	msr	SPSR_fc, r9
