	.sdata
	.align 2
	.type w1,@object
	.size w1,4
w1:	.word	1
	.type w2,@object
	.size w2,4
w2:	.word	2
	.type w3,@object
	.size w3,4
w3:	.word	3
	
	.text
	.align 2
l0:
	.set	noreorder

        li.d    $f2,1.10000000000000000000e0
        li.d    $f2,2.10000000000000000000e0
        li.d    $f2,3.10000000000000000000e0
        li.s    $f2,1.10000000000000000000e0
        li.s    $f2,2.10000000000000000000e0
        li.s    $f2,3.10000000000000000000e0

	.set	nomacro
	
	lw	$2,w1
	lw	$2,w2
	lw	$2,w3
