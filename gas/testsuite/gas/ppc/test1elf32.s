
	
	

 
 
 
 
 
 


	.section	".data"
dsym0:	.long	0xdeadbeef
dsym1:



	.section	".text"
	lwz	3,dsym0@l(3)
	lwz	3,dsym1@l(3)
	lwz	3,usym0@l(3)
	lwz	3,usym1@l(3)
	lwz	3,esym0@l(3)
	lwz	3,esym1@l(3)



	li	3,dsym1-dsym0
	li	3,dsym0-dsym1
	li	3,usym1-usym0
	li	3,usym0-usym1
	li	3,dsym0-usym0
	li	3,usym0-dsym0

	li	3,dsym0@l
	li	3,dsym0@h
	li	3,dsym0@ha


	li	3,usym0-usym1@l
	li	3,usym0-usym1@h
	li	3,usym0-usym1@ha


	lwz	3,dsym1-dsym0@l(4)

	lwz	3,.text@l(0)

	.section	".data"
usym0:	.long	0xcafebabe
usym1:

datpt:	.long	jk-.+10000000
dat0:	.long	jk-dat1
dat1:	.long	jk-dat1
dat2:	.long	jk-dat1

