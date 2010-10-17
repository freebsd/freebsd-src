# Motorola PowerPC BookE tests
#as: -a64 -mppc64 -mbooke64
	.machine	"ppc64"
	.csect .text[PR]
	.csect main[DS]
main:
	.csect .text[PR]
.main:
	tlbre   1, 2, 7
	tlbwe   5, 30, 3
	bce	1, 5, branch_target_1
	bcel	2, 6, branch_target_2
	bcea	3, 7, branch_target_3
	bcela	4, 8, branch_target_4
	bclre	5, 9
	bclrel	5, 10
	bcctre	8, 11
	bcctrel	8, 12
	be	branch_target_5
	bel	branch_target_6
	bea	branch_target_7
	bela	branch_target_8

branch_target_1:
	lbze	8, 8(9)
	lbzue	12, 4(15)
	lbzuxe	4, 6, 8
	lbzxe	3, 5, 7

branch_target_2:
	lde	5, 400(6)
	ldue	6, 452(7)
	ldxe	7, 8, 9
	lduxe	10, 11, 12

branch_target_3:
	lfde	12, 128(1)
	lfdue	1, 16(5)
	lfdxe	5, 1, 3
	lfduxe	6, 2, 4
	lfse	8, 48(9)
	lfsue	9, 68(10)
	lfsuxe	10, 4, 8
	lfsxe	9, 3, 7

branch_target_4:
	lhae	10, 50(5)
	lhaue	1, 5(3)	
	lhauxe	5, 1, 3
	lhaxe	29, 30, 31
	lhbrxe	1, 2, 3
	lhze	4, 18(3)
	lhzue	6, 20(9)
	lhzuxe	5, 7, 9
	lhzxe	9, 7, 5

branch_target_5:
	lwarxe	10, 15, 20
	lwbrxe	5, 10, 18
	lwze	28, 4(29)
	lwzue	8, 40(10)
	lwzuxe	3, 6, 9
	lwzxe	30, 29, 28

branch_target_6:
	dcbae	6, 7
	dcbfe	8, 9
	dcbie	10, 11
	dcbste	8, 30
	dcbte	6, 3, 1
	dcbtste	5, 4, 2
	dcbze	15, 14
	icbie	3, 4
	icbt	5, 8, 9	
	icbte	6, 10, 15
	mfapidi	5, 6
	tlbivax	7, 8
	tlbivaxe 9, 10	
	tlbsx	11, 12
	tlbsxe	13, 14

branch_target_7:
	adde64	1, 2, 3
	adde64o	4, 5, 6
	addme64 7, 8
	addme64o 9, 10
	addze64 11, 12
	addze64o 13, 14
	mcrxr64 5
	subfe64 15, 16, 17
	subfe64o 18, 19, 20
	subfme64 21, 22
	subfme64o 23, 24
	subfze64 25, 26
	subfze64o 27, 28

branch_target_8:
	stbe	1, 50(2)
	stbue	3, 40(4)
	stbuxe	5, 6, 7
	stbxe	8, 9, 10
	stdcxe.	11, 12, 13
	stde	14, 28(15)
	stdue	16, 20(17)
	stdxe	18, 19, 20
	stduxe	21, 22, 23
	stfde	1, 12(24)
	stfdue	2, 0(25)
	stfdxe	3, 26, 27
	stfduxe 4, 28, 29
	stfiwxe	5, 30, 31
	stfse	6, 24(30)
	stfsue	7, 20(29)
	stfsxe	8, 28, 27
	stfsuxe	9, 26, 25
	sthbrxe	24, 23, 22
	sthe	21, 30(20)
	sthue	19, 40(18)
	sthuxe	17, 16, 15
	sthxe	14, 13, 12
	stwbrxe	11, 10, 9
	stwcxe.	8, 7, 6
	stwe	5, 50(4)
	stwue	3, 40(2)
	stwuxe	1, 2, 3
	stwxe	4, 5, 6

	rfci
	wrtee	3
	wrteei	1
	mfdcrx	4, 5
	mfdcr	5, 234
	mtdcrx	6, 7
	mtdcr	432, 8
	msync
	dcba	9, 10
	mbar	0
