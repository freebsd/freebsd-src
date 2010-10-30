        .text
        .global main
main:
	#####################################
	# ASHUB cnt(left +)/cnt (right -), reg
	#####################################
        ashub   $7,r1
        ashub   $-7,r1
        ashub   $4,r1
        ashub   $-4,r1
        ashub	$-8,r1
        ashub   $3,r1
        ashub   $-3,r1
	#####################################
	# ASHUB reg, reg
	#####################################
        ashub   r2,r1
        ashub   r3,r4
        ashub   r5,r6
        ashub   r8,r10
	#####################################
	# ASHUW cnt(left +)/cnt (right -), reg
	#####################################
        ashuw   $7,r1
        ashuw   $-7,r1
        ashuw   $4,r1
        ashuw   $-4,r1
        ashuw	$8,r1
        ashuw	$-8,r1
        ashuw   $3,r1
        ashuw   $-3,r1
	#####################################
	# ASHUW reg, reg
	#####################################
        ashuw   r2,r1
        ashuw   r3,r4
        ashuw   r5,r6
        ashuw   r8,r10
	#####################################
	# ASHUD cnt(left +)/cnt (right -), regp
	#####################################
        ashud   $7, (r3,r2)
        ashud   $-7, (r3,r2)
        ashud   $8, (r3,r2)
        ashud   $-8, (r3,r2)
        ashud   $4, (r3,r2)
        ashud   $-4, (r3,r2)
        ashud   $12,(r3,r2)
        ashud   $-12,(r3,r2)
        ashud	$3,(r2,r1)
        ashud	$-3,(r2,r1)
	#####################################
	# ASHUD reg, regp
	#####################################
        ashud   r4,(r2,r1)
        ashud   r5,(r2,r1)
        ashud   r6,(r2,r1)
        ashud   r8,(r2,r1)
        ashud   r1,(r2,r1)
