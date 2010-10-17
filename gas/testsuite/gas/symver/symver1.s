 	.data
 	.symver bar,bar@@@version1
 	.symver bar,bar@@@version1
	.globl foo1
	.type foo1,object
foo1:
	.long foo
	.symver foo,foo@@@version1
	.symver foo1,foo1@@@version1
L_foo1:
	.size foo1,L_foo1-foo1
	.globl foo2
	.type foo2,object
foo2:
	.long foo
	.symver foo2,foo2@@version1
L_foo2:
	.size foo2,L_foo2-foo2
