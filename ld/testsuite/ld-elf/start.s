	.text
	.global _start
_start:
	.global __start
__start:
	.global start	/* Used by SH targets.  */
start:
	.global main	/* Used by HPPA targets.  */
main:
	.long 0
