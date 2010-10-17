! Relative linking.  Like the simple test, but mixing in use of
! "datalabel" and offsets to the global symbols into the previous
! combinations.
!
! More systematic testing datalabel references,
! igoring section difference, symbol definition type and offset presence:
! Datalabel reference plus:
! (datalabel other file, other file, same file, none)
! = (1, 2, 3, 4, 12, 13, 23, 123)
!
! Definition:
! (none, same file, other file) = (a, b, c)
!
! Combined: 
! = (a1, a2, a3, a4, a12, a13, a23, a123, b1, b2, b3, b4, b12,
!    b13, b23, b123, c1, c2, c3, c4, c12, c13, c23, c123)

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
	movi (datalabel file1data2) & 65535,r20
	.global file1text3
file1text3:
	movi file2text3 & 65535,r20
	.global file1text4
file1text4:
	movi file2data4 & 65535,r20
	.global file1text5
file1text5:
	movi unresolved1 & 65535,r40
	.global b1
b1:
	movi unresolved6 & 65535,r30
	.global b2
b2:
	movi (datalabel file1text1) & 65535,r10
	.global b3
b3:
	movi (datalabel file1text1 + 24) & 65535,r10
	.global b4
b4:
	movi (datalabel file1text5 + 8) & 65535, r40
	.global b12
b12:
	movi (datalabel file1data2 + 48) & 65535,r20
	.global b13
b13:
	movi file1data2 & 65535,r20
	.global b23
b23:
	movi (datalabel file2data4 + 16),r50
	.global b123
b123:
	movi (datalabel unresolved7) & 65535,r60
	.global oc1
oc1:
	movi (datalabel unresolved1) & 65535,r60
	.global oc2
oc2:
	nop
	.global oc3
oc3:
	nop
	.global oc4
oc4:
	nop
	.global oc12
oc12:
	nop
	.global oc13
oc13:
	nop
	.global oc23
oc23:
	nop
	.global oc123
oc123:
	nop

	.data
	.long 0
	.global file1data1
file1data1:
	.long 0
	.long datalabel file1data1 + 8
	.global file1data2
file1data2:
	.long file1text2
	.global file1data3
file1data3:
	.long file2data3
	.global file1data4
file1data4:
	.long file2text4
	.global file1data5
file1data5:
	.long unresolved2
	.long unresolved5
	.long datalabel unresolved6 + 40
	.long unresolved9

	.long datalabel a1
	.long a23
	.long b123
	.long c3
	.long c13
	.long datalabel a2
	.long datalabel a3
	.long datalabel a4
	.long datalabel a12
	.long datalabel a13
	.long datalabel a23
	.long datalabel a123
	.long datalabel b1
	.long datalabel b2
	.long a3
	.long a13
	.long datalabel b3
	.long datalabel b4
	.long datalabel b12
	.long datalabel b13
	.long a123
	.long b3
	.long b13
	.long b23
	.long datalabel b23
	.long datalabel b123
	.long datalabel c1
	.long datalabel c2
	.long datalabel c3
	.long c23
	.long c123
	.long datalabel c4
	.long datalabel c12
	.long datalabel c13
	.long datalabel c23
	.long datalabel c123


	.long datalabel oa1
	.long datalabel ob1
	.long ob123
	.long datalabel oc1
	.long oa2
	.long ob2
	.long oc2
	.long oa12
	.long datalabel oa12
	.long datalabel ob12
	.long ob12
	.long datalabel oc12
	.long oc12
	.long oa23
	.long datalabel oa13
	.long oc123
	.long datalabel ob13
	.long datalabel oc13
	.long ob23
	.long oc23
	.long oa123
	.long datalabel oa123
	.long datalabel ob123
	.long datalabel oc123
