! Test that .cranges are emitted:
!  1) Not for sections with single contents.
!  2) For data (through pseudo-ops) in SHmedia.
!  3) For mixed SHcompact and SHmedia sections.
!  4) For a mix of 2 and 3
!  5) For 4, repeated.
!
! Use section contents that need relaxing to strengthen the check that the
! .cranges implementation handles this correctly.  Use different sizes for
! each contents part.
!

! The .text section has only SHmedia contents, and should not get a
! .cranges descriptor.
	.mode SHmedia
	.text
	nop
shmedia:
	movi 42,r45
	movi shmediaend-shmedia,r46
shmediaend:
	nop

! Likewise the SHcompact section.
	.mode SHcompact
	.section .text.compact,"ax"
	nop
shcompact:
	mov #42,r0
	bt shcompactend
	nop
shcompactend:
	nop

! This section has SHmedia code followed by data.  There should be two
! .cranges descriptors.  Note that we put the .mode directive *after* the
! section change.  It should not matter.
	.section .text.shmediaanddata,"ax"
	.mode SHmedia
shmedia_data_code:
	movi 42,r45
	movi shmedia_data_code_end-shmedia_data_code,r46
shmedia_data_code_end:
	.long 0x6ff0fff0
	.long shmedia_dataend-shmedia_data_code
	.long 50
shmedia_dataend:

! This section mixes SHcompact and SHmedia code.  There should be two
! .cranges descriptors.
	.section .text.codemix,"ax"
shmedia_compact_code:
	movi 42,r45
	nop
	nop
	movi shmedia_compact_code_end-shmedia_compact_code,r46
	nop
	nop
shmedia_compact_code_end:
	.mode SHcompact
compact_code:
	nop
compact:
	mov #40,r0
	nop
	nop
	bt compactend
	nop
compactend:
	nop

! This section mixes SHcompact and SHmedia code, and has a constant
! section after the SHmedia code and one after the SHcompact code.  There
! should be three or four .cranges descriptors, depending on whether one
! is emitted for the SHcompact constant pool: there's normally one such
! after each SHcompact function.
	.mode SHmedia
	.section .text.codemixconst,"ax"
	nop
shmedia_compact_code2:
	movi 42,r45
	nop
	nop
	movi shmedia_compact_code_end2-shmedia_compact_code2,r46
	nop
	nop
	.long 0x6ff0fff0
	.long 0x6ff0fff0
	.long 0x6ff00000
	.long 0xfff0
	.long 0x6ff0fff0
	.long 0x6ff0fff0
	.long 0
mediapoollabel:
	.long mediapoollabel2-shmedia_compact_code2
mediapoolend:
shmedia_compact_code_end2:
	.mode SHcompact
compact_code2:
	nop
compact2:
	mov #43,r0
	nop
	nop
	bt compactend2
	nop
	nop
	nop
compactend2:
	nop
	.space 102,0
	.long 0
mediapoollabel2:
	.long mediapoolend2-compact2
mediapoolend2:

! This section is like the previous, but repeated twice and adjusted to
! keep different sizes of each part.
	.mode SHmedia
	.section .text.codemixconst2,"ax"
	nop
shmedia_compact_code3:
	movi 42,r45
	nop
	nop
	nop
	nop
	nop
	nop
	movi shmedia_compact_code_end3-shmedia_compact_code3,r46
	.long 0x6ff0fff0
	.long 0
	.long 0
	.long 0
	.long 0
	.long 0
	.long 0
	.long 0
	.long 0
mediapoollabel3a:
	.long mediapoollabel3a-shmedia_compact_code3
mediapoolend3a:
shmedia_compact_code_end3:
	.mode SHcompact
compact_code3:
	nop
compact3:
	mov #44,r0
	nop
	nop
	bt compactend3
	nop
	nop
	nop
	nop
	nop
compactend3:
	nop
	.word 9
	.word 0x900
	.space 198,0
	.long 0
mediapoollabel3:
	.long mediapoolend3-compact3
mediapoolend3:
	.mode SHmedia
	nop
shmedia_compact_code4:
	movi 43,r45
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	movi shmedia_compact_code_end4-shmedia_compact_code4,r46
	.long 0x6ff0fff0
	.space 20,0
mediapoollabel4a:
	.long mediapoolend4a-shmedia_compact_code4
mediapoolend4a:
shmedia_compact_code_end4:
	.mode SHcompact
compact_code4:
	nop
compact4:
	mov #14,r0
	nop
	nop
	bt compactend4
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
compactend4:
	nop
	.space 298,0
	.long 0
mediapoollabel4:
	.long mediapoolend4-compact4
mediapoolend4:
