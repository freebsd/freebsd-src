	.text

	ldctl	r0,fcw
	ldctl	fcw,r0
	ldctl	r0,refresh
	ldctl	refresh,r0
	ldctl	r0,psapseg
	ldctl	psapseg,r0
	ldctl	r0,psapoff
	ldctl	psapoff,r0
	ldctl	r0,psap
	ldctl	psap,r0
	ldctl	r0,nspseg
	ldctl	nspseg,r0
	ldctl	r0,nspoff
	ldctl	nspoff,r0
	ldctl	r0,nsp
	ldctl	nsp,r0

	LDCTL	R0,FCW
	LDCTL	FCW,R0
	LDCTL	R0,REFRESH
	LDCTL	REFRESH,R0
	LDCTL	R0,PSAPSEG
	LDCTL	PSAPSEG,R0
	LDCTL	R0,PSAPOFF
	LDCTL	PSAPOFF,R0
	LDCTL	R0,PSAP
	LDCTL	PSAP,R0
	LDCTL	R0,NSPSEG
	LDCTL	NSPSEG,R0
	LDCTL	R0,NSPOFF
	LDCTL	NSPOFF,R0
	LDCTL	R0,NSP
	LDCTL	NSP,R0
