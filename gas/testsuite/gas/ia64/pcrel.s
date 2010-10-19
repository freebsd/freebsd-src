.explicit
.global esym

.altmacro

.macro begin n, attr
 .section .&n, attr, @progbits
 .align 16
_&n:
.endm
.macro end n
 .align 16
_e&n:
.endm

.macro m1 op, opnd1
 .align 16
	op		opnd1 _e&op - _&op
.endm
.macro m2 op, opnd1
 .align 16
	op		opnd1 @pcrel(esym)
.endm
.macro m3 op, opnd1
 .align 16
	op		opnd1 esym - _&op
.endm
.macro m4 op, opnd1
 .align 16
	op		opnd1 esym - .
.endm
.macro m5 op, opnd1
 .align 16
	op		opnd1 esym - _e&op
.endm
.macro m6 op, opnd1
 .align 16
	op		opnd1 0
.endm

begin	mov, "ax"
	m1	mov, r2 =
	;;
	m2	mov, r2 =
	;;
	m3	mov, r2 =
	;;
	m4	mov, r2 =
	;;
	m5	mov, r2 =
	;;
	m6	mov, r2 =
	;;
end mov

begin	movl, "ax"
	m1	movl, r2 =
	;;
	m2	movl, r2 =
	;;
	m3	movl, r2 =
	;;
	m4	movl, r2 =
	;;
	m5	movl, r2 =
	;;
	m6	movl, r2 =
	;;
end movl

begin data8, "a"
	m1	data8
	m2	data8
	m3	data8
	m4	data8
	m5	data8
	m6	data8
end data8

begin data4, "a"
	m1	data4
	m2	data4
	m3	data4
	m4	data4
	m5	data4
	m6	data4
end data4
