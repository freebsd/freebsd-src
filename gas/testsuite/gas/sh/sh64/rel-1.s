! Test pc-relative relocations in MOVI and MOVI expansion.

	.mode SHmedia
	.text
start:
	movi data1 - datalabel $,r10
	movi (data2 - datalabel $) & 65535,r10
	movi ((data3 - datalabel $) >> 0) & 65535,r10
	movi ((data4 - datalabel $) >> 16) & 65535,r10
	movi data5 + 8 - datalabel $,r10
	movi (data6 + 16 - datalabel $) & 65535,r10
	movi ((data7 + 12 - datalabel $) >> 0) & 65535,r10
	movi ((data8 + 4 - datalabel $) >> 16) & 65535,r10

	movi othertext1 - datalabel $,r10
	movi (othertext2 - datalabel $) & 65535,r10
	movi ((othertext3 - datalabel $) >> 0) & 65535,r10
	movi ((othertext4 - datalabel $) >> 16) & 65535,r10
	movi othertext5 + 8 - datalabel $,r10
	movi (othertext6 + 16 - datalabel $) & 65535,r10
	movi ((othertext7 + 12 - datalabel $) >> 0) & 65535,r10
	movi ((othertext8 + 4 - datalabel $) >> 16) & 65535,r10

	movi extern1 - datalabel $,r10
	movi (extern2 - datalabel $) & 65535,r10
	movi ((extern3 - datalabel $) >> 0) & 65535,r10
	movi ((extern4 - datalabel $) >> 16) & 65535,r10
	movi extern5 + 8 - datalabel $,r10
	movi (extern6 + 16 - datalabel $) & 65535,r10
	movi ((extern7 + 12 - datalabel $) >> 0) & 65535,r10
	movi ((extern8 + 4 - datalabel $) >> 16) & 65535,r10

	movi gdata1 - datalabel $,r10
	movi (gdata2 - datalabel $) & 65535,r10
	movi ((gdata3 - datalabel $) >> 0) & 65535,r10
	movi ((gdata4 - datalabel $) >> 16) & 65535,r10
	movi gdata5 + 8 - datalabel $,r10
	movi (gdata6 + 16 - datalabel $) & 65535,r10
	movi ((gdata7 + 12 - datalabel $) >> 0) & 65535,r10
	movi ((gdata8 + 4 - datalabel $) >> 16) & 65535,r10

	movi gothertext1 - datalabel $,r10
	movi (gothertext2 - datalabel $) & 65535,r10
	movi ((gothertext3 - datalabel $) >> 0) & 65535,r10
	movi ((gothertext4 - datalabel $) >> 16) & 65535,r10
	movi gothertext5 + 8 - datalabel $,r10
	movi (gothertext6 + 16 - datalabel $) & 65535,r10
	movi ((gothertext7 + 12 - datalabel $) >> 0) & 65535,r10
	movi ((gothertext8 + 4 - datalabel $) >> 16) & 65535,r10

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
