# source file to test assembly of MIPS DSP ASE Rev2 for MIPS32 instructions

	.set noreorder
	.set noat

	.text
text_label:

	absq_s.qb	$0,$1
	addu.ph		$1,$2,$3
	addu_s.ph	$2,$3,$4
	adduh.qb	$3,$4,$5
	adduh_r.qb	$4,$5,$6
	append		$5,$6,0
	append		$5,$6,31
	balign		$6,$7,0
	balign		$6,$7,1
	balign		$6,$7,2
	balign		$6,$7,3
	cmpgdu.eq.qb	$6,$7,$8
	cmpgdu.lt.qb	$7,$8,$9
	cmpgdu.le.qb	$8,$9,$10
	dpa.w.ph	$ac0,$9,$10
	dps.w.ph	$ac1,$10,$11
	madd		$ac2,$11,$12
	maddu		$ac3,$12,$13
	msub		$ac0,$13,$14
	msubu		$ac1,$14,$15
	mul.ph		$15,$16,$17
	mul_s.ph	$16,$17,$18
	mulq_rs.w	$17,$18,$19
	mulq_s.ph	$18,$19,$20
	mulq_s.w	$19,$20,$21
	mulsa.w.ph	$ac2,$20,$21
	mult		$ac3,$21,$22
	multu		$ac0,$22,$23
	precr.qb.ph	$23,$24,$25
	precr_sra.ph.w	$24,$25,0
	precr_sra.ph.w	$24,$25,31
	precr_sra_r.ph.w	$25,$26,0
	precr_sra_r.ph.w	$25,$26,31
	prepend		$26,$27,0
	prepend		$26,$27,31
	shra.qb		$27,$28,0
	shra.qb		$27,$28,7
	shra_r.qb	$28,$29,0
	shra_r.qb	$28,$29,7
	shrav.qb	$29,$30,$31
	shrav_r.qb	$30,$31,$0
	shrl.ph		$31,$0,0
	shrl.ph		$31,$0,15
	shrlv.ph	$0,$1,$2
	subu.ph		$1,$2,$3
	subu_s.ph	$2,$3,$4
	subuh.qb	$3,$4,$5
	subuh_r.qb	$4,$5,$6
	addqh.ph        $5,$6,$7
	addqh_r.ph      $6,$7,$8
	addqh.w         $7,$8,$9
	addqh_r.w       $8,$9,$10
	subqh.ph        $9,$10,$11
	subqh_r.ph      $10,$11,$12
	subqh.w         $11,$12,$13
	subqh_r.w       $12,$13,$14
	dpax.w.ph       $ac1,$13,$14
	dpsx.w.ph       $ac2,$14,$15
	dpaqx_s.w.ph    $ac3,$15,$16
	dpaqx_sa.w.ph   $ac0,$16,$17
	dpsqx_s.w.ph    $ac1,$17,$18
	dpsqx_sa.w.ph   $ac2,$18,$19

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8
