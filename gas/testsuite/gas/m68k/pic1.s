	.text
	.globl _foo
_foo:
	leal %pc@(_i), %a0
	leal %pc@(_i-.), %a1
