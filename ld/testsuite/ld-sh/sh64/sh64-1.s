! Test that all common kinds of relocs get right for simple use.
! Auxiliary part.
	.text
	.mode SHmedia
	.global foo
	.global bar
foo:
	pt/l xyzzy,tr3
bar:
	nop

	.data
	.global baz
baz:
	.long foobar
	.long bar
	.global baz2
baz2:
	.long xyzzy
foobar:	.long	baz
