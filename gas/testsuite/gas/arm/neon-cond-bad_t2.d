# name: Conditions in Neon instructions, Thumb mode (illegal in ARM).
# as: -mfpu=neon -I$srcdir/$subdir
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section \.text:
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> ef22 0152 	vorreq	q0, q1, q1
0[0-9a-f]+ <[^>]+> ef21 0111 	vorreq	d0, d1, d1
0[0-9a-f]+ <[^>]+> ef80 0050 	vmoveq\.i32	q0, #0	; 0x00000000
0[0-9a-f]+ <[^>]+> ef80 0010 	vmoveq\.i32	d0, #0	; 0x00000000
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> ee20 2b10 	vmoveq\.32	d0\[1\], r2
0[0-9a-f]+ <[^>]+> ec42 1b10 	vmoveq	d0, r1, r2
0[0-9a-f]+ <[^>]+> ee11 2b10 	vmoveq\.32	r2, d1\[0\]
0[0-9a-f]+ <[^>]+> ec51 0b12 	vmoveq	r0, r1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ff01 0d12 	vmuleq\.f32	d0, d1, d2
0[0-9a-f]+ <[^>]+> ff02 0d54 	vmuleq\.f32	q0, q1, q2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ef01 0d12 	vmlaeq\.f32	d0, d1, d2
0[0-9a-f]+ <[^>]+> ef02 0d54 	vmlaeq\.f32	q0, q1, q2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ef21 0d12 	vmlseq\.f32	d0, d1, d2
0[0-9a-f]+ <[^>]+> ef22 0d54 	vmlseq\.f32	q0, q1, q2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ef01 0d02 	vaddeq\.f32	d0, d1, d2
0[0-9a-f]+ <[^>]+> ef02 0d44 	vaddeq\.f32	q0, q1, q2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ef21 0d02 	vsubeq\.f32	d0, d1, d2
0[0-9a-f]+ <[^>]+> ef22 0d44 	vsubeq\.f32	q0, q1, q2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ffb9 0701 	vabseq\.f32	d0, d1
0[0-9a-f]+ <[^>]+> ffb9 0742 	vabseq\.f32	q0, q1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ffb9 0781 	vnegeq\.f32	d0, d1
0[0-9a-f]+ <[^>]+> ffb9 07c2 	vnegeq\.f32	q0, q1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ffbb 0701 	vcvteq\.s32\.f32	d0, d1
0[0-9a-f]+ <[^>]+> ffbb 0742 	vcvteq\.s32\.f32	q0, q1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ffbb 0781 	vcvteq\.u32\.f32	d0, d1
0[0-9a-f]+ <[^>]+> ffbb 07c2 	vcvteq\.u32\.f32	q0, q1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ffbb 0601 	vcvteq\.f32\.s32	d0, d1
0[0-9a-f]+ <[^>]+> ffbb 0642 	vcvteq\.f32\.s32	q0, q1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ffbb 0681 	vcvteq\.f32\.u32	d0, d1
0[0-9a-f]+ <[^>]+> ffbb 06c2 	vcvteq\.f32\.u32	q0, q1
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> ee80 1b10 	vdupeq\.32	d0, r1
0[0-9a-f]+ <[^>]+> eea0 1b10 	vdupeq\.32	q0, r1
0[0-9a-f]+ <[^>]+> ffb4 0c01 	vdupeq\.32	d0, d1\[0\]
0[0-9a-f]+ <[^>]+> ffbc 0c41 	vdupeq\.32	q0, d1\[1\]
