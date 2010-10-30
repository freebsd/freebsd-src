# name: Neon optional register operands
# as: -mfpu=neon
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> f3022746 	vabd\.u8	q1, q1, q3
0[0-9a-f]+ <[^>]+> f26cc0c6 	vhadd\.s32	q14, q14, q3
0[0-9a-f]+ <[^>]+> f2222144 	vrhadd\.s32	q1, q1, q2
0[0-9a-f]+ <[^>]+> f22aa24e 	vhsub\.s32	q5, q5, q7
0[0-9a-f]+ <[^>]+> f3186446 	vshl\.u16	q3, q3, q4
0[0-9a-f]+ <[^>]+> f32ca45a 	vqshl\.u32	q5, q5, q6
0[0-9a-f]+ <[^>]+> f20ee170 	vand	q7, q7, q8
0[0-9a-f]+ <[^>]+> f30ee170 	veor	q7, q7, q8
0[0-9a-f]+ <[^>]+> f3b5a14a 	vceq\.i16	q5, q5, #0
0[0-9a-f]+ <[^>]+> f31aa85a 	vceq\.i16	q5, q5, q5
0[0-9a-f]+ <[^>]+> f3b5a24a 	vclt\.s16	q5, q5, #0
0[0-9a-f]+ <[^>]+> f3b5a34c 	vabs\.s16	q5, q6
0[0-9a-f]+ <[^>]+> f3b57388 	vneg\.s16	d7, d8
0[0-9a-f]+ <[^>]+> f3b97708 	vabs\.f32	d7, d8
0[0-9a-f]+ <[^>]+> f3f927e4 	vneg\.f32	q9, q10
0[0-9a-f]+ <[^>]+> f2211a03 	vpmax\.s32	d1, d1, d3
0[0-9a-f]+ <[^>]+> f2255a17 	vpmin\.s32	d5, d5, d7
0[0-9a-f]+ <[^>]+> f3011f03 	vpmax\.f32	d1, d1, d3
0[0-9a-f]+ <[^>]+> f3255f07 	vpmin\.f32	d5, d5, d7
0[0-9a-f]+ <[^>]+> f2122b46 	vqdmulh\.s16	q1, q1, q3
0[0-9a-f]+ <[^>]+> f3255b07 	vqrdmulh\.s32	d5, d5, d7
0[0-9a-f]+ <[^>]+> f3922c6d 	vqdmulh\.s16	q1, q1, d5\[3\]
0[0-9a-f]+ <[^>]+> f2122056 	vqadd\.s16	q1, q1, q3
0[0-9a-f]+ <[^>]+> f2255017 	vqadd\.s32	d5, d5, d7
0[0-9a-f]+ <[^>]+> f2222944 	vmla\.i32	q1, q1, q2
0[0-9a-f]+ <[^>]+> f2133b14 	vpadd\.i16	d3, d3, d4
0[0-9a-f]+ <[^>]+> f3266948 	vmls\.i32	q3, q3, q4
0[0-9a-f]+ <[^>]+> f3022e54 	vacge\.f32	q1, q1, q2
0[0-9a-f]+ <[^>]+> f3266e58 	vacgt\.f32	q3, q3, q4
0[0-9a-f]+ <[^>]+> f30cae5a 	vacge\.f32	q5, q6, q5
0[0-9a-f]+ <[^>]+> f320eede 	vacgt\.f32	q7, q8, q7
0[0-9a-f]+ <[^>]+> f32ee370 	vcge\.u32	q7, q7, q8
0[0-9a-f]+ <[^>]+> f32ee360 	vcgt\.u32	q7, q7, q8
0[0-9a-f]+ <[^>]+> f320e3de 	vcge\.u32	q7, q8, q7
0[0-9a-f]+ <[^>]+> f320e3ce 	vcgt\.u32	q7, q8, q7
0[0-9a-f]+ <[^>]+> f3a22102 	vaddw\.u32	q1, q1, d2
0[0-9a-f]+ <[^>]+> f2a66304 	vsubw\.s32	q3, q3, d4
0[0-9a-f]+ <[^>]+> f2244856 	vtst\.32	q2, q2, q3
0[0-9a-f]+ <[^>]+> f2011f12 	vrecps\.f32	d1, d1, d2
0[0-9a-f]+ <[^>]+> f29c2052 	vshr\.s16	q1, q1, #4
0[0-9a-f]+ <[^>]+> f28b4254 	vrshr\.s8	q2, q2, #5
0[0-9a-f]+ <[^>]+> f39a6156 	vsra\.u16	q3, q3, #6
0[0-9a-f]+ <[^>]+> f39a8358 	vrsra\.u16	q4, q4, #6
0[0-9a-f]+ <[^>]+> f3954554 	vsli\.16	q2, q2, #5
0[0-9a-f]+ <[^>]+> f3bff69f 	vqshlu\.s64	d15, d15, #63
0[0-9a-f]+ <[^>]+> f2b55306 	vext\.8	d5, d5, d6, #3
0[0-9a-f]+ <[^>]+> f3042746 	vabd\.u8	q1, q2, q3
0[0-9a-f]+ <[^>]+> f262c0c6 	vhadd\.s32	q14, q9, q3
0[0-9a-f]+ <[^>]+> f22a2144 	vrhadd\.s32	q1, q5, q2
0[0-9a-f]+ <[^>]+> f220a2ce 	vhsub\.s32	q5, q8, q7
0[0-9a-f]+ <[^>]+> f31a6448 	vshl\.u16	q3, q4, q5
0[0-9a-f]+ <[^>]+> f322a45c 	vqshl\.u32	q5, q6, q1
0[0-9a-f]+ <[^>]+> f200e1dc 	vand	q7, q8, q6
0[0-9a-f]+ <[^>]+> f300e1dc 	veor	q7, q8, q6
0[0-9a-f]+ <[^>]+> f3b5a146 	vceq\.i16	q5, q3, #0
0[0-9a-f]+ <[^>]+> f316a85a 	vceq\.i16	q5, q3, q5
0[0-9a-f]+ <[^>]+> f3b5a246 	vclt\.s16	q5, q3, #0
0[0-9a-f]+ <[^>]+> f2231a20 	vpmax\.s32	d1, d3, d16
0[0-9a-f]+ <[^>]+> f2275a34 	vpmin\.s32	d5, d7, d20
0[0-9a-f]+ <[^>]+> f3031f07 	vpmax\.f32	d1, d3, d7
0[0-9a-f]+ <[^>]+> f32c5f07 	vpmin\.f32	d5, d12, d7
0[0-9a-f]+ <[^>]+> f2162b60 	vqdmulh\.s16	q1, q3, q8
0[0-9a-f]+ <[^>]+> f3275b09 	vqrdmulh\.s32	d5, d7, d9
0[0-9a-f]+ <[^>]+> f39c2c6d 	vqdmulh\.s16	q1, q6, d5\[3\]
0[0-9a-f]+ <[^>]+> f21620d6 	vqadd\.s16	q1, q11, q3
0[0-9a-f]+ <[^>]+> f227503f 	vqadd\.s32	d5, d7, d31
0[0-9a-f]+ <[^>]+> f2242962 	vmla\.i32	q1, q2, q9
0[0-9a-f]+ <[^>]+> f21a3b94 	vpadd\.i16	d3, d26, d4
0[0-9a-f]+ <[^>]+> f328694a 	vmls\.i32	q3, q4, q5
0[0-9a-f]+ <[^>]+> f3082e54 	vacge\.f32	q1, q4, q2
0[0-9a-f]+ <[^>]+> f3226e58 	vacgt\.f32	q3, q1, q4
0[0-9a-f]+ <[^>]+> f30cae72 	vacge\.f32	q5, q6, q9
0[0-9a-f]+ <[^>]+> f320eed2 	vacgt\.f32	q7, q8, q1
0[0-9a-f]+ <[^>]+> f320e3d6 	vcge\.u32	q7, q8, q3
0[0-9a-f]+ <[^>]+> f320e3c6 	vcgt\.u32	q7, q8, q3
0[0-9a-f]+ <[^>]+> f326e370 	vcge\.u32	q7, q3, q8
0[0-9a-f]+ <[^>]+> f326e360 	vcgt\.u32	q7, q3, q8
0[0-9a-f]+ <[^>]+> f3aa2102 	vaddw\.u32	q1, q5, d2
0[0-9a-f]+ <[^>]+> f2a26304 	vsubw\.s32	q3, q1, d4
0[0-9a-f]+ <[^>]+> f22648d6 	vtst\.32	q2, q11, q3
0[0-9a-f]+ <[^>]+> f20e1f92 	vrecps\.f32	d1, d30, d2
0[0-9a-f]+ <[^>]+> f29c207a 	vshr\.s16	q1, q13, #4
0[0-9a-f]+ <[^>]+> f28b4272 	vrshr\.s8	q2, q9, #5
0[0-9a-f]+ <[^>]+> f39a6152 	vsra\.u16	q3, q1, #6
0[0-9a-f]+ <[^>]+> f3dae358 	vrsra\.u16	q15, q4, #6
0[0-9a-f]+ <[^>]+> f3954556 	vsli\.16	q2, q3, #5
0[0-9a-f]+ <[^>]+> f3bff6b7 	vqshlu\.s64	d15, d23, #63
0[0-9a-f]+ <[^>]+> f2b25386 	vext\.8	d5, d18, d6, #3
