! Relative linking, second file.

! fileFsectionN, with F in rel-F.s, and N in:
! 1 - Same file and section.
! 2 - Same file, different section.
! 3 - Other file, same section.
! 4 - Other file, other section.

	.mode SHmedia
	.text
	.global start2
start2:
	nop
	.global file2text1
file2text1:
	nop
	movi file2text1 & 65535,r10
	.global file2text2
file2text2:
	movi file2data2 & 65535,r20
	.global file2text3
file2text3:
	movi file1text3 & 65535,r20
	.global file2text4
file2text4:
	movi file1data4 & 65535,r20
	movi unresolved1 & 65535,r30
	movi unresolved3 & 65535,r30

	.data
	.long 0
	.global file2data1
file2data1:
	.long 0
	.long file2data1
	.global file2data2
file2data2:
	.long file2text2
	.global file2data3
file2data3:
	.long file1data3
	.global file2data4
file2data4:
	.long file1text4
	.long unresolved2
	.long unresolved4
