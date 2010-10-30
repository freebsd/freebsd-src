# Check that gas recognizes both wdebug and wdebug.l.
	.text
	.globl	foo
foo:
	wdebug (%a0)
	wdebug.l (%a0)
