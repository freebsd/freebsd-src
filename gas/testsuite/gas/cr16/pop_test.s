        .text
        .global main
main:
	####################
	# pop uimm3 regr RA
	####################
	pop $1,r7,RA
	pop $2,r6,RA
	pop $3,r5,RA
	pop $4,r4,RA
	pop $5,r3,RA
	pop $6,r2,RA
	pop $7,r1,RA
	#################
	# pop uimm3 regr
	#################
	pop $1,r7
	pop $2,r6
	pop $3,r5
	pop $4,r4
	pop $5,r3
	pop $6,r2
	pop $7,r1
	##########
	# pop RA
	##########
	pop RA
