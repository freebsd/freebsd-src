        .text
	.arch armv5t
        .arm
one:
        blx     foo
        blx     foo2

        .thumb
        .type foo, %function
        .thumb_func
foo:
        nop
        .type foo2, %function
        .thumb_func
foo2:
        nop
