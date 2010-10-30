# name: VFP/Neon overlapping instructions
# as: -mfpu=vfp
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> ec410b10 	vmov	d0, r0, r1
0[0-9a-f]+ <[^>]+> ec410b10 	vmov	d0, r0, r1
0[0-9a-f]+ <[^>]+> ec510b10 	vmov	r0, r1, d0
0[0-9a-f]+ <[^>]+> ec510b10 	vmov	r0, r1, d0
0[0-9a-f]+ <[^>]+> ec900b09 	fldmiax	r0, {d0-d3}
0[0-9a-f]+ <[^>]+> ed300b09 	fldmdbx	r0!, {d0-d3}
0[0-9a-f]+ <[^>]+> ec800b09 	fstmiax	r0, {d0-d3}
0[0-9a-f]+ <[^>]+> ed200b09 	fstmdbx	r0!, {d0-d3}
0[0-9a-f]+ <[^>]+> ed900b00 	vldr	d0, \[r0\]
0[0-9a-f]+ <[^>]+> ed900b00 	vldr	d0, \[r0\]
0[0-9a-f]+ <[^>]+> ed800b00 	vstr	d0, \[r0\]
0[0-9a-f]+ <[^>]+> ed800b00 	vstr	d0, \[r0\]
0[0-9a-f]+ <[^>]+> ec900b08 	vldmia	r0, {d0-d3}
0[0-9a-f]+ <[^>]+> ec900b08 	vldmia	r0, {d0-d3}
0[0-9a-f]+ <[^>]+> ed300b08 	vldmdb	r0!, {d0-d3}
0[0-9a-f]+ <[^>]+> ed300b08 	vldmdb	r0!, {d0-d3}
0[0-9a-f]+ <[^>]+> ec800b08 	vstmia	r0, {d0-d3}
0[0-9a-f]+ <[^>]+> ec800b08 	vstmia	r0, {d0-d3}
0[0-9a-f]+ <[^>]+> ed200b08 	vstmdb	r0!, {d0-d3}
0[0-9a-f]+ <[^>]+> ed200b08 	vstmdb	r0!, {d0-d3}
0[0-9a-f]+ <[^>]+> ee300b10 	vmov\.32	r0, d0\[1\]
0[0-9a-f]+ <[^>]+> ee300b10 	vmov\.32	r0, d0\[1\]
0[0-9a-f]+ <[^>]+> ee100b10 	vmov\.32	r0, d0\[0\]
0[0-9a-f]+ <[^>]+> ee100b10 	vmov\.32	r0, d0\[0\]
0[0-9a-f]+ <[^>]+> ee200b10 	vmov\.32	d0\[1\], r0
0[0-9a-f]+ <[^>]+> ee200b10 	vmov\.32	d0\[1\], r0
0[0-9a-f]+ <[^>]+> ee000b10 	vmov\.32	d0\[0\], r0
0[0-9a-f]+ <[^>]+> ee000b10 	vmov\.32	d0\[0\], r0
