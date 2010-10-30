# name: Neon single and multiple register loads and stores
# as: -mfpu=neon
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section \.text:
0[0-9a-f]+ <[^>]+> ec922b02 	vldmia	r2, {d2}
0[0-9a-f]+ <[^>]+> ec922b04 	vldmia	r2, {d2-d3}
0[0-9a-f]+ <[^>]+> ec924b08 	vldmia	r2, {d4-d7}
0[0-9a-f]+ <[^>]+> ecd28b10 	vldmia	r2, {d24-d31}
0[0-9a-f]+ <[^>]+> ec923b20 	vldmia	r2, {d3-d18}
0[0-9a-f]+ <[^>]+> ec922b02 	vldmia	r2, {d2}
0[0-9a-f]+ <[^>]+> ec922b04 	vldmia	r2, {d2-d3}
0[0-9a-f]+ <[^>]+> ec924b08 	vldmia	r2, {d4-d7}
0[0-9a-f]+ <[^>]+> ecd28b10 	vldmia	r2, {d24-d31}
0[0-9a-f]+ <[^>]+> ec923b20 	vldmia	r2, {d3-d18}
0[0-9a-f]+ <[^>]+> ecb22b02 	vldmia	r2!, {d2}
0[0-9a-f]+ <[^>]+> ecb22b04 	vldmia	r2!, {d2-d3}
0[0-9a-f]+ <[^>]+> ecb24b08 	vldmia	r2!, {d4-d7}
0[0-9a-f]+ <[^>]+> ecf28b10 	vldmia	r2!, {d24-d31}
0[0-9a-f]+ <[^>]+> ecb23b20 	vldmia	r2!, {d3-d18}
0[0-9a-f]+ <[^>]+> ed322b02 	vldmdb	r2!, {d2}
0[0-9a-f]+ <[^>]+> ed322b04 	vldmdb	r2!, {d2-d3}
0[0-9a-f]+ <[^>]+> ed324b08 	vldmdb	r2!, {d4-d7}
0[0-9a-f]+ <[^>]+> ed728b10 	vldmdb	r2!, {d24-d31}
0[0-9a-f]+ <[^>]+> ed323b20 	vldmdb	r2!, {d3-d18}
0[0-9a-f]+ <[^>]+> ec822b02 	vstmia	r2, {d2}
0[0-9a-f]+ <[^>]+> ec822b04 	vstmia	r2, {d2-d3}
0[0-9a-f]+ <[^>]+> ec824b08 	vstmia	r2, {d4-d7}
0[0-9a-f]+ <[^>]+> ecc28b10 	vstmia	r2, {d24-d31}
0[0-9a-f]+ <[^>]+> ec823b20 	vstmia	r2, {d3-d18}
0[0-9a-f]+ <[^>]+> ec822b02 	vstmia	r2, {d2}
0[0-9a-f]+ <[^>]+> ec822b04 	vstmia	r2, {d2-d3}
0[0-9a-f]+ <[^>]+> ec824b08 	vstmia	r2, {d4-d7}
0[0-9a-f]+ <[^>]+> ecc28b10 	vstmia	r2, {d24-d31}
0[0-9a-f]+ <[^>]+> ec823b20 	vstmia	r2, {d3-d18}
0[0-9a-f]+ <[^>]+> eca22b02 	vstmia	r2!, {d2}
0[0-9a-f]+ <[^>]+> eca22b04 	vstmia	r2!, {d2-d3}
0[0-9a-f]+ <[^>]+> eca24b08 	vstmia	r2!, {d4-d7}
0[0-9a-f]+ <[^>]+> ece28b10 	vstmia	r2!, {d24-d31}
0[0-9a-f]+ <[^>]+> eca23b20 	vstmia	r2!, {d3-d18}
0[0-9a-f]+ <[^>]+> ed222b02 	vstmdb	r2!, {d2}
0[0-9a-f]+ <[^>]+> ed222b04 	vstmdb	r2!, {d2-d3}
0[0-9a-f]+ <[^>]+> ed224b08 	vstmdb	r2!, {d4-d7}
0[0-9a-f]+ <[^>]+> ed628b10 	vstmdb	r2!, {d24-d31}
0[0-9a-f]+ <[^>]+> ed223b20 	vstmdb	r2!, {d3-d18}
0[0-9a-f]+ <backward> 000001f4 	.*
0[0-9a-f]+ <[^>]+> eddf6b0b 	vldr	d22, \[pc, #44\]	; 0[0-9a-f]+ <forward>
0[0-9a-f]+ <[^>]+> ed935b00 	vldr	d5, \[r3\]
0[0-9a-f]+ <[^>]+> ed135b01 	vldr	d5, \[r3, #-4\]
0[0-9a-f]+ <[^>]+> ed935b01 	vldr	d5, \[r3, #4\]
0[0-9a-f]+ <[^>]+> ed835b00 	vstr	d5, \[r3\]
0[0-9a-f]+ <[^>]+> ed035b01 	vstr	d5, \[r3, #-4\]
0[0-9a-f]+ <[^>]+> ed835b01 	vstr	d5, \[r3, #4\]
0[0-9a-f]+ <[^>]+> ed935b00 	vldr	d5, \[r3\]
0[0-9a-f]+ <[^>]+> ed135b40 	vldr	d5, \[r3, #-256\]
0[0-9a-f]+ <[^>]+> ed935b40 	vldr	d5, \[r3, #256\]
0[0-9a-f]+ <[^>]+> ed835b00 	vstr	d5, \[r3\]
0[0-9a-f]+ <[^>]+> ed035b40 	vstr	d5, \[r3, #-256\]
0[0-9a-f]+ <[^>]+> ed835b40 	vstr	d5, \[r3, #256\]
0[0-9a-f]+ <forward> 000002bc 	.*
0[0-9a-f]+ <[^>]+> ed1f7b11 	vldr	d7, \[pc, #-68\]	; 0[0-9a-f]+ <backward>
