! Relative linking, simple files with global symbols but nothing really
! strange.  Reference from same and other file to .text and .data in
! different combinations.

! fileFsectionN, with F in rel-F.s, and N in:
! 1 - Same file and section.
! 2 - Same file, different section.
! 3 - Other file, same section.
! 4 - Other file, other section.

	.mode SHmedia
	.text
	.global start
start:
	nop
	.global file1text1
file1text1:
	nop
	movi file1text1 & 65535,r10
	.global file1text2
file1text2:
	movi file1data2 & 65535,r20
	.global file1text3
file1text3:
	movi file2text3 & 65535,r20
	.global file1text4
file1text4:
	movi file2data4 & 65535,r20
	movi unresolved1 & 65535,r40
	movi unresolved6 & 65535,r30

	.data
	.long 0
	.global file1data1
file1data1:
	.long 0
	.long file1data1
	.global file1data2
file1data2:
	.long file1text2
	.global file1data3
file1data3:
	.long file2data3
	.global file1data4
file1data4:
	.long file2text4
	.long unresolved2
	.long unresolved5
