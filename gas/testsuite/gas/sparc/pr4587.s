	.section .data
	.align 4
zero: .single 0.0

	.section .text
	.align 4
	.global main
main:
    save %sp, -96, %sp

    ! Zero-out the first FP register
    set zero, %l0
    ld [%l0], %f0

    ! Compare it to itself
    ! The third reg (%f0) will cause a segfault in as
    ! fcmps only takes two regs... this should be illegal operand error
    fcmps %f0, %f0, %f0

    ! Return 0
    ret
    restore %g0, %g0, %o0
