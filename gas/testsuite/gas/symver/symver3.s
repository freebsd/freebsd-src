 	.data
	.globl foo1
	.type foo1,object
foo1:
	.long foo
	.symver foo,foo
L_foo1:
	.size foo1,L_foo1-foo1
