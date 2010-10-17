! Test gc-sections and datalabel references.
!
! Datalabel reference to symbol in section .text2 should
! prevent .text2 from being discarded.
! Section .spurious can be discarded.
	.mode SHmedia

	.text
	.global start
	.global foo
start:	.long datalabel foo

	.section .text2,"ax"
foo:	.long 23
		
	.section .spurious,"ax"
	.long 17
