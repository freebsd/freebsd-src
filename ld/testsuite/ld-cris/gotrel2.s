	.text
	.weak undefweak
	.global _start
_start:
	move.d	[$r0+undefweak:GOT],$r3
