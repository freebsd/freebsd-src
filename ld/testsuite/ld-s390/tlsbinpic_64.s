	.section ".tdata", "awT", @progbits
	.globl sg1, sg2, sg3, sg4, sg5, sg6, sg7, sg8
	.globl sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
	.hidden sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
	.hidden sh1, sh2
sg1:	.long 17
sg2:	.long 18
sg3:	.long 19
sg4:	.long 20
sg5:	.long 21
sg6:	.long 22
sg7:	.long 23
sg8:	.long 24
sl1:	.long 65
sl2:	.long 66
sl3:	.long 67
sl4:	.long 68
sl5:	.long 69
sl6:	.long 70
sl7:	.long 71
sl8:	.long 72
sh1:	.long 257
sh2:	.long 258
sh3:	.long 259
sh4:	.long 260
sh5:	.long 261
sh6:	.long 262
sh7:	.long 263
sh8:	.long 264
	.text
	.globl	fn2
	.type	fn2,@function
fn2:
	/* Function prolog */
	stmg	%r6,%r14,48(%r15)
	bras	%r13,.LTN1
	/* Literal pool */
.LT1:
.LC2:
	.quad	sG1@tlsgd
.LC3:
	.quad	sG2@tlsgd
.LC4:
	.quad	sg1@tlsgd
.LC5:
	.quad	sl1@tlsgd
.LC6:
	.quad	sh1@tlsgd
.LC7:
	.quad	sl1@tlsldm
.LC8:
	.quad	sl1@dtpoff
.LC9:
	.quad	sl2@dtpoff
.LC10:
	.quad	sh1@tlsldm
.LC11:
	.quad	sh1@dtpoff
.LC12:
	.quad	sh2@dtpoff
.LC13:
	.quad	sG2@gotntpoff
.LC14:
	.quad	sg1@gotntpoff
.LC15:
	.quad	sl1@gotntpoff
.LC16:
	.quad	sh1@gotntpoff
.LTN1:
	/* Function prolog */
	lgr	%r14,%r15
	aghi	%r15,-160
	larl	%r12,_GLOBAL_OFFSET_TABLE_
	stg	%r14,0(%r14)

	/* Extract TCB */
	ear	%r9,%a0
	sllg	%r9,%r4,32
	ear	%r9,%a1

	/* GD -> IE because variable is not defined in executable */
	lg	%r12,.LC2-.LT1(%r13)
	brasl	%r14,__tls_get_offset@plt:tls_gdcall:sG1
	la	%r2,0(%r2,%r9)

	/* GD -> IE because variable is not defined in executable where
	   the variable is referenced through IE too */
	lg	%r2,.LC3-.LT1(%r13)
	brasl	%r14,__tls_get_offset@plt:tls_gdcall:sG2
	la	%r2,0(%r2,%r9)

	/* GD -> LE with global variable defined in executable */
	lg	%r2,.LC4-.LT1(%r13)
	brasl	%r14,__tls_get_offset@plt:tls_gdcall:sg1
	la	%r2,0(%r2,%r9)

	/* GD -> LE with local variable defined in executable */
	lg	%r2,.LC5-.LT1(%r13)
	brasl	%r14,__tls_get_offset@plt:tls_gdcall:sl1
	la	%r2,0(%r2,%r9)

	/* GD -> LE with hidden variable defined in executable */
	lg	%r2,.LC6-.LT1(%r13)
	brasl	%r14,__tls_get_offset@plt:tls_gdcall:sh1
	la	%r2,0(%r2,%r9)

	/* LD -> LE */
	lg	%r2,.LC7-.LT1(%r13)
	brasl	%r14,__tls_get_offset@plt:tls_ldcall:sl1
	la	%r3,0(%r2,%r9)
	lg	%r4,.LC8-.LT1(%r13)
	la	%r5,0(%r4,%r3)
	lg	%r4,.LC9-.LT1(%r13)
	la	%r5,0(%r4,%r3)

	/* LD -> LE against hidden variables */
	lg	%r2,.LC10-.LT1(%r13)
	brasl	%r14,__tls_get_offset@plt:tls_ldcall:sh1
	la	%r3,0(%r2,%r9)
	lg	%r4,.LC11-.LT1(%r13)
	la	%r5,0(%r4,%r3)
	lg	%r4,.LC12-.LT1(%r13)
	la	%r5,0(%r4,%r3)

	/* IE against global var  */
	lg	%r3,.LC13-.LT1(%r13)
	lg	%r3,0(%r3,%r12):tls_load:sG2
	la	%r3,0(%r3,%r9)

	/* IE -> LE against global var defined in exec */
	lg	%r3,.LC14-.LT1(%r13)
	lg	%r4,0(%r3,%r12):tls_load:sg1
	la	%r5,0(%r4,%r9)

	/* IE -> LE against local var */
	lg	%r3,.LC15-.LT1(%r13)
	lg	%r4,0(%r3,%r12):tls_load:sl2
	la	%r5,0(%r4,%r9)

	/* IE -> LE against hidden var */
	lg	%r3,.LC16-.LT1(%r13)
	lg	%r4,0(%r3,%r12):tls_load:sh1
	la	%r5,0(%r4,%r9)

	/* IE against global var with larl got access */
	larl	%r3,sG3@indntpoff
	lg	%r3,0(%r3,%r12):tls_load:sG3
	la	%r3,0(%r3,%r9)

	/* IE against global var defined in exec with larl got access */
	larl	%r3,sg3@indntpoff
	lg	%r4,0(%r3,%r12):tls_load:sg3
	la	%r5,0(%r4,%r9)

	/* IE against local var with larl got access */
	larl	%r3,sl3@indntpoff
	lg	%r4,0(%r3,%r12):tls_load:sl3
	la	%r5,0(%r4,%r9)
	
	/* IE against hidden var with larl got access */
	larl	%r3,sh3@indntpoff
	lg	%r4,0(%r3,%r12):tls_load:sh3
	la	%r5,0(%r4,%r9)

	/* IE against global var with small got access (no optimization) */
	lg	%r3,sG3@gotntpoff(%r12)
	la	%r3,0(%r3,%r9)

	/* IE against global var defined in exec with small got access
	   (no optimization) */
	lg	%r3,sg3@gotntpoff(%r12)
	la	%r3,0(%r3,%r9)

	/* IE against local var with small got access (no optimization) */
	lg	%r3,sl3@gotntpoff(%r12)
	la	%r3,0(%r3,%r9)

	/* IE against hidden var with small got access (no optimization) */
	lg	%r3,sh3@gotntpoff(%r12)
	la	%r3,0(%r3,%r9)
	
	/* Function epilog */
	lmg	%r6,%r14,208(%r15)
	br	%r14
