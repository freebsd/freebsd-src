        .text
        .global main
main:
	####################
	# popret uimm3 regr RA
	####################
	popret $1,r7,RA
	popret $2,r6,RA
	popret $3,r5,RA
	popret $4,r4,RA
	popret $5,r3,RA
	popret $6,r2,RA
	popret $7,r1,RA
	#################
	# popret uimm3 regr
	#################
	popret $1,r7
	popret $2,r6
	popret $3,r5
	popret $4,r4
	popret $5,r3
	popret $6,r2
	popret $7,r1
	##########
	# popret RA
	##########
	popret RA
