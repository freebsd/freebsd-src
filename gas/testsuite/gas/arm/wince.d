#objdump: -dr --prefix-addresses --show-raw-insn
#name: ARM WinCE basic tests
#as: -mcpu=arm7m -EL
#source: wince.s
#not-skip: *-wince-*

# Some WinCE specific tests.

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <global_data> 00000007 	andeq	r0, r0, r7
			0: ARM_32	global_data
0+004 <global_sym> e1a00000 	nop			\(mov r0,r0\)
0+008 <global_sym\+0x4> e1a00000 	nop			\(mov r0,r0\)
0+000c <global_sym\+0x8> e1a00000 	nop			\(mov r0,r0\)
0+010 <global_sym\+0xc> eafffffb 	b	f+ff8 <global_sym\+0xf+ff4>
			10: ARM_26D	global_sym\+0xf+ffc
0+018 <global_sym\+0x14> ebfffffa 	bl	f+ff4 <global_sym\+0xf+ff0>
			14: ARM_26D	global_sym\+0xf+ffc
0+01c <global_sym\+0x18> 0afffff9 	beq	f+ff0 <global_sym\+0xf+fec>
			18: ARM_26D	global_sym\+0xf+ffc
0+020 <global_sym\+0x1c> eafffff8 	b	0+008 <global_sym\+0x4>
0+024 <global_sym\+0x20> ebfffff7 	bl	0+008 <global_sym\+0x4>
0+028 <global_sym\+0x24> 0afffff6 	beq	0+008 <global_sym\+0x4>
0+02c <global_sym\+0x28> eafffff5 	b	0+008 <global_sym\+0x4>
0+030 <global_sym\+0x2c> ebfffff4 	bl	0+008 <global_sym\+0x4>
0+034 <global_sym\+0x30> e51f0034 	ldr	r0, \[pc, #-52\]	; 0+008 <global_sym\+0x4>
0+038 <global_sym\+0x34> e51f0038 	ldr	r0, \[pc, #-56\]	; 0+008 <global_sym\+0x4>
0+03c <global_sym\+0x38> e51f003c 	ldr	r0, \[pc, #-60\]	; 0+008 <global_sym\+0x4>
