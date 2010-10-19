# Co-Processor instructions.
 .data
foodata: .word 42
	 .text
footext:

	.global cpi
cpi:
cpi $0x2, $0x1234
cpi $3, $0x8765, $0x4321

	.global mtcr
mtcr:
mtcr $0xf, r1, c14

	.global mfcr
mfcr:
mfcr $3, c7, r2

	.global mtcsr
mtcsr:
mtcsr $0x2, r5, cs1

	.global mfcsr
mfcsr:
mfcsr $01, cs12, ra

	.global ldcr
ldcr:
ldcr $1, r3, c8

	.global stcr
stcr:
stcr $2, c11, r4

	.global ldcsr
ldcsr:
ldcsr $4, r6, cs12

	.global stcsr
stcsr:
stcsr $7, cs10, r13

	.global loadmcr
loadmcr:
loadmcr $3, r1, {c2,c3,c5}

	.global stormcr
stormcr:
stormcr $15, ra, {c10,c9,c7,c4}

	.global loadmcsr
loadmcsr:
loadmcsr $12, r8, {cs7, cs8, cs9, cs10, cs11}

	.global stormcsr
stormcsr:
stormcsr $9, r9, {cs10,cs7,cs4}

	.global bcop
bcop:
bcop $7, $3, 0x90
bcop $6, $12, -0xbcdfe

	.global cpdop
cpdop:
cpdop $3, $2, r4, r5
cpdop $7, $10, r1, r2, $0x1234

	.global mtpr
mtpr:
mtpr r0 , hi

	.global mfpr
mfpr:
mfpr lo , r5
mfpr uhi , r10

	.global cinv
cinv:
cinv [i,d,u,b]

