# SPDX-License-Identifier: BSD-2-Clause
# Four XRGB8888 pixels per iteration. Unpack, blend, round, repack.
pxor xmm7, xmm7
movq xmm2, qword ptr [{src}]
punpcklqdq xmm2, xmm2
movd xmm3, dword ptr [{inv}]
pshufd xmm3, xmm3, 0
movd xmm4, dword ptr [{bias}]
pshufd xmm4, xmm4, 0
movd xmm6, dword ptr [{mask}]
pshufd xmm6, xmm6, 0
2:
movdqu xmm0, xmmword ptr [{dst}]
movdqa xmm1, xmm0
punpcklbw xmm1, xmm7
punpckhbw xmm0, xmm7
pmullw xmm1, xmm3
pmullw xmm0, xmm3
paddw xmm1, xmm2
paddw xmm0, xmm2
paddw xmm1, xmm4
paddw xmm0, xmm4
movdqa xmm5, xmm1
psrlw xmm5, 8
paddw xmm1, xmm5
movdqa xmm5, xmm0
psrlw xmm5, 8
paddw xmm0, xmm5
psrlw xmm1, 8
psrlw xmm0, 8
packuswb xmm1, xmm0
pand xmm1, xmm6
movdqu xmmword ptr [{dst}], xmm1
add {dst}, 16
dec {blocks}
jnz 2b
