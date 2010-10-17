! Like rel-3.s, but as with rel-2 vs. rel-1, using "$", not "datalabel $"
! as self expression.

	.mode SHmedia
	.text
start:
	movi datalabel data1 - $,r10
	movi (datalabel data2 - $) & 65535,r10
	movi ((datalabel data3 - $) >> 0) & 65535,r10
	movi ((datalabel data4 - $) >> 16) & 65535,r10
	movi datalabel data5 + 8 - $,r10
	movi (datalabel data6 + 16 - $) & 65535,r10
	movi ((datalabel data7 + 12 - $) >> 0) & 65535,r10
	movi ((datalabel data8 + 4 - $) >> 16) & 65535,r10

	movi datalabel othertext1 - $,r10
	movi (datalabel othertext2 - $) & 65535,r10
	movi ((datalabel othertext3 - $) >> 0) & 65535,r10
	movi ((datalabel othertext4 - $) >> 16) & 65535,r10
	movi datalabel othertext5 + 8 - $,r10
	movi (datalabel othertext6 + 16 - $) & 65535,r10
	movi ((datalabel othertext7 + 12 - $) >> 0) & 65535,r10
	movi ((datalabel othertext8 + 4 - $) >> 16) & 65535,r10

	movi datalabel extern1 - $,r10
	movi (datalabel extern2 - $) & 65535,r10
	movi ((datalabel extern3 - $) >> 0) & 65535,r10
	movi ((datalabel extern4 - $) >> 16) & 65535,r10
	movi datalabel extern5 + 8 - $,r10
	movi (datalabel extern6 + 16 - $) & 65535,r10
	movi ((datalabel extern7 + 12 - $) >> 0) & 65535,r10
	movi ((datalabel extern8 + 4 - $) >> 16) & 65535,r10

	movi datalabel gdata1 - $,r10
	movi (datalabel gdata2 - $) & 65535,r10
	movi ((datalabel gdata3 - $) >> 0) & 65535,r10
	movi ((datalabel gdata4 - $) >> 16) & 65535,r10
	movi datalabel gdata5 + 8 - $,r10
	movi (datalabel gdata6 + 16 - $) & 65535,r10
	movi ((datalabel gdata7 + 12 - $) >> 0) & 65535,r10
	movi ((datalabel gdata8 + 4 - $) >> 16) & 65535,r10

	movi datalabel gothertext1 - $,r10
	movi (datalabel gothertext2 - $) & 65535,r10
	movi ((datalabel gothertext3 - $) >> 0) & 65535,r10
	movi ((datalabel gothertext4 - $) >> 16) & 65535,r10
	movi datalabel gothertext5 + 8 - $,r10
	movi (datalabel gothertext6 + 16 - $) & 65535,r10
	movi ((datalabel gothertext7 + 12 - $) >> 0) & 65535,r10
	movi ((datalabel gothertext8 + 4 - $) >> 16) & 65535,r10

	.section .othertext,"ax"
x:
	nop
othertext1:
	nop
othertext2:
	nop
othertext3:
	nop
othertext4:
	nop
othertext5:
	nop
othertext6:
	nop
othertext7:
	nop
othertext8:
	nop
	.global gothertext1
gothertext1:
	nop
	.global gothertext2
gothertext2:
	nop
	.global gothertext3
gothertext3:
	nop
	.global gothertext4
gothertext4:
	nop
	.global gothertext5
gothertext5:
	nop
	.global gothertext6
gothertext6:
	nop
	.global gothertext7
gothertext7:
	nop
	.global gothertext8
gothertext8:
	nop

	.data
y:
	.long 0
data1:
	.long 0
data2:
	.long 0
data3:
	.long 0
data4:
	.long 0
data5:
	.long 0
data6:
	.long 0
data7:
	.long 0
data8:
	.long 0
	.global gdata1
gdata1:
	.long 0
	.global gdata2
gdata2:
	.long 0
	.global gdata3
gdata3:
	.long 0
	.global gdata4
gdata4:
	.long 0
	.global gdata5
gdata5:
	.long 0
	.global gdata6
gdata6:
	.long 0
	.global gdata7
gdata7:
	.long 0
	.global gdata8
gdata8:
	.long 0
