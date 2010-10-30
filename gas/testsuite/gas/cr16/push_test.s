        .text
        .global main
main:
	####################
	# push uimm3 regr RA
	####################
	push $1,r7,RA
	push $2,r6,RA
	push $3,r5,RA
	push $4,r4,RA
	push $5,r3,RA
	push $6,r2,RA
	push $7,r1,RA
#push $6,r12,RA
	#push $7,r13,RA
	#push $7,r12,RA
	#push $8,r12,RA
	#################
	# push uimm3 regr
	#################
	push $1,r7
	push $2,r6
	push $3,r5
	push $4,r4
	push $5,r3
	push $6,r2
	push $7,r1
	push $6,r12
	#push $7,r13
	#push $7,r12
	#push $8,r12
	#push $6,r13
	##########
	# push RA
	##########
	#push r1
	#push r4
	#push r9
	push ra
	push RA
