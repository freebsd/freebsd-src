        .text
        .global main
main:
	###########################
	# LSHB cnt(right -), reg
	###########################
        lshb   $7,r1
        lshb   $-7,r1
        lshb   $4,r1
        lshb   $-4,r1
        lshb	$-8,r1
        lshb   $3,r1
        lshb   $-3,r1
	###########################
	# LSHB reg, reg
	###########################
        lshb   r2,r1
        lshb   r3,r4
        lshb   r5,r6
        lshb   r8,r10
	###########################
	# LSHW cnt (right -), reg
	###########################
        lshw   $7,r1
        lshw   $-7,r1
        lshw   $4,r1
        lshw   $-4,r1
        lshw	$8,r1
        lshw	$-8,r1
        lshw   $3,r1
        lshw   $-3,r1
	##########################
	# LSHW reg, reg
	##########################
        lshw   r2,r1
        lshw   r3,r4
        lshw   r5,r6
        lshw   r8,r10
	###########################
	# LSHD cnt (right -), regp
	############################
        lshd   $7, (r3,r2)
        lshd   $-7, (r3,r2)
        lshd   $8, (r3,r2)
        lshd   $-8, (r3,r2)
        lshd   $4, (r3,r2)
        lshd   $-4, (r3,r2)
        lshd   $12,(r3,r2)
        lshd   $-12,(r3,r2)
        lshd	$3,(r2,r1)
        lshd	$-3,(r2,r1)
	#################
	# LSHD reg, regp
	#################
        lshd   r4,(r2,r1)
        lshd   r5,(r2,r1)
        lshd   r6,(r2,r1)
        lshd   r8,(r2,r1)
        lshd   r1,(r2,r1)
