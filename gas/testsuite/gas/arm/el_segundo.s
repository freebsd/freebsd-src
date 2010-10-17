# el_segundo.s
#
# Tests that we generate the right code for v5e instructions.
# This is not a functional test, although it can be linked.
# (The section at the rear is non-Coyanosa stuff for comparison.)
# To verify a compiler, do:
#	<gcc build area>/gcc/as el_segundo.s -o _temp.o
#	<gcc build area>/binutils/objdump -dr _temp.o >! _temp.d
#	diff _temp.d el_segundo.d

	.section	.rdata
	.align	0
.LC0:
	.ascii	"some data\000"

	.text
	.global main
#	.type main,function
	.align	0

main:
	smlabbgt r0,r1,r2,r3
	smlabb r0,r1,r2,r3
	smlatb r0,r1,r2,r3
	smlabt r0,r1,r2,r3
	smlatt r0,r1,r2,r3

	smlawbgt r0,r1,r2,r3
	smlawb r0,r1,r2,r3
	smlawt r0,r1,r2,r3

	smlalbbgt r0,r1,r2,r3
	smlalbb r0,r1,r2,r3
	smlaltb r0,r1,r2,r3
	smlalbt r0,r1,r2,r3
	smlaltt r0,r1,r2,r3

	smulbbgt r0,r1,r2
	smulbb r0,r1,r2
	smultb r0,r1,r2
	smulbt r0,r1,r2
	smultt r0,r1,r2

	smulwbgt r0,r1,r2
	smulwb r0,r1,r2
	smulwt r0,r1,r2

	qaddgt r0,r1,r2
	qadd r0,r1,r2

	qdadd r0,r1,r2
	qsub r0,r1,r2
	qdsub r0,r1,r2
	qsub r0,r1,r2
