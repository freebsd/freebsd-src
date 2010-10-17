	.section ".tdata", "awT", @progbits
	.align 16
	.global x#, y#, z#, a#, b#, c#
	.protected a#, b#, c#
	.type	x#,@object
	.size	x#,4
x:	data4	1
	.type	y#,@object
	.size	y#,4
y:	data4	2
	.type	z#,@object
	.size	z#,4
z:	data4	3
	.align	8
	.type	a#,@object
	.size	a#,8
a:	data8	4
	.type	b#,@object
	.size	b#,8
b:	data8	5
	.type	c#,@object
	.size	c#,1
c:	data1	6

	.text
	.align 16
	.global foo#
	.proc foo#
foo:
	.prologue
	alloc r36 = ar.pfs, 0, 5, 3, 0
	.body
	addl	loc0 = @ltoff(@tprel(x)), gp;;
	ld8	loc0 = [loc0];;
	add	loc1 = loc0, r13;;

	mov	r2 = r13;;
	addl	loc1 = @tprel(y), r2;;

	mov	loc0 = gp
	addl	out0 = @ltoff(@dtpmod(z)), gp
	addl	out1 = @ltoff(@dtprel(z)), gp;;
	ld8	out0 = [out0]
	ld8	out1 = [out1]
	br.call.sptk.many	b0 = __tls_get_addr;;
	mov	gp = loc0;;

	addl	out0 = @ltoff(@dtpmod(a)), gp
	addl	out1 = @dtprel(a), r0;;
	ld8	out0 = [out0]
	br.call.sptk.many	b0 = __tls_get_addr;;
	mov	gp = loc0;;

	addl	out0 = @ltoff(@dtpmod(b)), gp
	mov	out1 = r0;;
	ld8	out0 = [out0]
	br.call.sptk.many	b0 = __tls_get_addr;;
	mov	gp = loc0
	mov	r2 = ret0;;
	addl	loc1 = @dtprel(b), r2
	addl	loc2 = @dtprel(c), r2

	br.ret.sptk.many b0
	.endp foo#
