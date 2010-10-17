! Relative linking with datalabel use, second file.  Much like rel-2.s

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
	movi datalabel unresolved8 & 65535,r50
	movi datalabel unresolved9 & 65535,r50
	movi datalabel file1text1 & 65535,r40
	movi datalabel file1data2 & 65535,r40
	movi datalabel file1data3 & 65535,r40
	.global c1
c1:
	nop
	.global c2
c2:
	nop
	.global c3
c3:
	nop
	.global c4
c4:
	nop
	.global c12
c12:
	nop
	.global c13
c13:
	nop
	.global c23
c23:
	nop
	.global c123
c123:
	nop

	.global ob1
ob1:
	nop
	.global ob2
ob2:
	nop
	.global ob3
ob3:
	nop
	.global ob4
ob4:
	nop
	.global ob12
ob12:
	nop
	.global ob13
ob13:
	nop
	.global ob23
ob23:
	nop
	.global ob123
ob123:
	nop

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

	.long datalabel oa1
	.long datalabel oa2
	.long datalabel oa3
	.long oa13
	.long oc13
	.long datalabel oa4
	.long datalabel oa12
	.long datalabel oa13
	.long datalabel oa23
	.long oa23
	.long oa123
	.long oc3
	.long datalabel oa123
	.long datalabel ob1
	.long datalabel ob2
	.long datalabel ob3
	.long datalabel ob4
	.long oa3
	.long oc23
	.long oc123
	.long datalabel ob12
	.long datalabel ob13
	.long ob13
	.long ob23
	.long datalabel ob23
	.long datalabel ob123
	.long datalabel oc1
	.long ob3
	.long ob123
	.long datalabel oc2
	.long datalabel oc3
	.long datalabel oc4
	.long datalabel oc12
	.long datalabel oc13
	.long datalabel oc23
	.long datalabel oc123

	.long datalabel a1
	.long c2
	.long b23
	.long datalabel b1
	.long datalabel c1
	.long datalabel a12
	.long a2
	.long b2
	.long datalabel b12
	.long datalabel c12
	.long b123
	.long c123
	.long datalabel a13
	.long datalabel b13
	.long c23
	.long a123
	.long datalabel c13
	.long datalabel a123
	.long c12
	.long a23
	.long datalabel b123
	.long a12
	.long b12
	.long datalabel c123
