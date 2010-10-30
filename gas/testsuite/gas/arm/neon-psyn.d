# name: Neon programmers syntax
# as: -mfpu=neon
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> f2144954 	vmul\.i16	q2, q2, q2
0[0-9a-f]+ <[^>]+> f2a33862 	vmul\.i32	d3, d3, d2\[1\]
0[0-9a-f]+ <[^>]+> f2233912 	vmul\.i32	d3, d3, d2
0[0-9a-f]+ <[^>]+> f2222803 	vadd\.i32	d2, d2, d3
0[0-9a-f]+ <[^>]+> f3924a4a 	vmull\.u16	q2, d2, d2\[1\]
0[0-9a-f]+ <[^>]+> f2910061 	vmla\.i16	d0, d1, d1\[2\]
0[0-9a-f]+ <[^>]+> f2910061 	vmla\.i16	d0, d1, d1\[2\]
0[0-9a-f]+ <[^>]+> f2255805 	vadd\.i32	d5, d5, d5
0[0-9a-f]+ <[^>]+> f2275117 	vorr	d5, d7, d7
0[0-9a-f]+ <[^>]+> ee021b70 	vmov\.16	d2\[1\], r1
0[0-9a-f]+ <[^>]+> ee251b10 	vmov\.32	d5\[1\], r1
0[0-9a-f]+ <[^>]+> ec432b15 	vmov	d5, r2, r3
0[0-9a-f]+ <[^>]+> ee554b30 	vmov\.s8	r4, d5\[1\]
0[0-9a-f]+ <[^>]+> ec565b15 	vmov	r5, r6, d5
0[0-9a-f]+ <[^>]+> f396a507 	vabal\.u16	q5, d6, d7
0[0-9a-f]+ <[^>]+> f3bb2744 	vcvt\.s32\.f32	q1, q2
0[0-9a-f]+ <[^>]+> f3bb4e15 	vcvt\.f32\.u32	d4, d5, #5
0[0-9a-f]+ <[^>]+> f3bc7c05 	vdup\.32	d7, d5\[1\]
0[0-9a-f]+ <[^>]+> f3ba1904 	vtbl\.8	d1, {d10-d11}, d4
0[0-9a-f]+ <[^>]+> f4aa698f 	vld2\.32	{d6\[1\],d7\[1\]}, \[sl\]
0[0-9a-f]+ <[^>]+> f4aa476f 	vld4\.16	{d4\[1\],d6\[1\],d8\[1\],d10\[1\]}, \[sl\]
0[0-9a-f]+ <[^>]+> f4aa6e4f 	vld3\.16	{d6\[\]-d8\[\]}, \[sl\]
0[0-9a-f]+ <[^>]+> ee100b30 	vmov\.s16	r0, d0\[0\]
0[0-9a-f]+ <[^>]+> f42a604f 	vld4\.16	{d6-d9}, \[sl\]
0[0-9a-f]+ <[^>]+> f4aa266f 	vld3\.16	{d2\[1\],d4\[1\],d6\[1\]}, \[sl\]
0[0-9a-f]+ <[^>]+> f3b47908 	vtbl\.8	d7, {d4-d5}, d8
0[0-9a-f]+ <[^>]+> f3142156 	vbsl	q1, q2, q3
0[0-9a-f]+ <[^>]+> f3032e04 	vcge\.f32	d2, d3, d4
0[0-9a-f]+ <[^>]+> f3b52083 	vcge\.s16	d2, d3, #0
0[0-9a-f]+ <[^>]+> ee823b30 	vdup\.16	d2, r3
