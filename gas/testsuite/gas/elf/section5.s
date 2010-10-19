	.section	.test0
	.section	.test1, "", %progbits
	.section	.test2
	.section	.test3, "aw"
	.section	.test4, "aw", %nobits

	.section	.test1, "aw", %nobits
test1:	.long	test1

	.section	.test2, "w"
test2:	.long	test2

	.section	.test3, "aw", %progbits
test3:	.long	test3

	.section	.test4, "aw"

	.section	.data, "a"

	.section	.bss, "a"

	.section	.data, "aw", %nobits

	.section	.bss, "aw", %progbits
