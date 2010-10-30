        .arch armv7a
        .syntax unified
	.text
func:
	nop
	movw r0, #0

	.arch armv7
	.thumb
	nop
	movw r0, #0
	.arm
	nop
