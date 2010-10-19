.macro	m1 v1, v2
	LOCAL l1, l2
label&v1:
l1:	.byte	v1
label&v2:
l2:	.byte	v2
.endm

.macro	m2 v1, v2
	m1 %(v1), %(v2-v1)
.endm

.macro	m3 str
	.ascii	&str
.endm

	.data

m2	1, 3
m2	9, 27

m3	"abc"
m3	<"1", "23">

	.noaltmacro

.macro	m4 str
	.ascii	"&str"
.endm

m4	"!!<>'"

	.altmacro

m3	"!!<>'"
