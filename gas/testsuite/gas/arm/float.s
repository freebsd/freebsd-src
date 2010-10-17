.text
.align 0
	mvfe	f0, f1
	mvfeqe	f3, f5
	mvfeqd	f4, #1.0
	mvfs	f4, f7
	mvfsp	f0, f1
	mvfdm	f3, f4
	mvfez	f7, f7

	adfe	f0, f1, #2.0
	adfeqe	f1, f2, #0.5
	adfsm	f3, f4, f5
	
	sufd	f0, f0, #2.0
	sufs	f1, f2, #10.0
	sufneez f3, f4, f5

	rsfs	f1, f1, #0.0
	rsfdp	f3, f0, #5.0
	rsfled	f7, f6, f0

	mufd	f0, f0, f0
	mufez	f1, f2, #3.0
	mufals	f0, f0, #4.0

	dvfd	f0, f0, #1.0000
	dvfez	f0, f1, #10e0
	dvfmism f3, f4, f5

	rdfe	f0, f1, #1.0e1
	rdfs	f3, f7, #0f1
	rdfccdp	f4, f4, f3

	powd	f0, f2, f3
	pows	f1, f3, #0e1e1
	powcsez	f4, f7, #1

	rpws	f7, f6, f7
	rpweqd	f0, f1, f2
	rpwem	f2, f2, f3

	rmfd	f1, f2, #3
	rmfvss	f3, f4, f4
	rmfep	f4, f7, f0

	fmls	f0, f1, f2
	fmleqs	f1, f3, f5
	fmlplsz	f4, f6, f0

	fdvs	f1, f3, #10
	fdvsp	f0, f1, f2
	fdvhssm	f4, f4, f4

	frds	f1, f1, #1.0
	frdgts	f2, f1, f0
	frdgtsz	f4, f4, f5

	pold	f0, f1, f2
	polsz	f4, f6, #3.0
	poleqe	f5, f6, f7

	mnfs	f0, f1
	mnfd	f0, #3.0
	mnfez	f0, #4.0
	mnfeqez f0, f5
	mnfsp	f0, f4
	mnfdm	f1, f7

	absd	f0, f1
	abssp	f1, #3.0
	abseqe	f4, f5

	rnds	f1, f2
	rndd	f3, f4
	rndeqez	f6, #4.0

	sqts	f5, f5
	sqtdp	f6, f6
	sqtplez f7, f6

	logs	f0, #10
	loge	f0, #0f10
	lognedz	f0, f1

	lgne	f1, f2
	lgndz	f1, f3
	lgnvcs	f3, f4

	exps	f1, f3
	expem	f3, #10.0
	exppld	f6, f7

	sind	f0, f1
	sinsm	f1, f2
	singte	f4, #5

	cosd	f1, f3
	cosem	f4, f5
	cosnedp	f6, f1

	tane	f1, f5
	tansz	f4, f7
	tangedz	f1, #4.0

	asne	f4, f5
	asnsp	f6, #5e-1
	asnmidz	f5, f5

	acss	f5, f6
	acsd	f6, f0
	acshsem	f1, #0.05e1

	atne	f0, f5
	atnsz	f1, #5
	atnltd	f3, f2

	urde	f5, f4
	nrme	f6, f5
	nrmpldz	f7, f5

	fltsp	f0, r8
	flte	f1, r0
	flteqdz	f5, r7

	fix	r0, f1
	fixz	r1, f7
	fixcsm	r5, f5

	wfc	r0
	wfs	r1
	rfseq	r2
	rfc	r4

	cmf	f0, #1
	cmf	f1, f2
	cmfeq	f0, f1

	cnf	f0, #3
	cnf	f1, #0.5
	cnfvs	f3, f4

	cmfe	f0, f1
	cmfeeq	f1, f2
	cmfeqe	f3, #5.0

	cnfe	f1, f3
	cnfeeq	f3, f4
	cnfeqe	f4, f7
	cnfale	f4, #5.0

	lfm	f0, 4, [r0]
	lfm	f0, 4, [r0, #0]
	lfm	f1, 4, [r1, #64]
	sfm	f2, 4, [r14, #1020]!
	sfmeq	f7, 3, [r8], #-1020

	lfmfd	f6, 2, [r15]
	sfmea	f7, 1, [r8]!
	lfmeqea	f5, 4, [r6]
	sfmnefd	f4, 3, [r2]
	sfmnefd	f4, 3, [r2]!
