	.section ".tdata", "awT", @progbits
	.globl sg1, sg2, sg3, sg4, sg5, sg6, sg7, sg8
	.globl sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
	.hidden sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8
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
	.ent	fn2
fn2:
	.frame	$sp, 16, $26, 0
	ldgp	$gp, 0($27)
	subq	$sp, 16, $sp
	stq	$26, 0($sp)
	.prologue 1
	
	/* GD */
	lda	$16, sG1($gp)			!tlsgd!1
	ldq	$27, __tls_get_addr($gp)	!literal!1
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsgd!1
	ldgp	$gp, 0($26)

	/* GD against local symbol */
	lda	$16, sl2($gp)			!tlsgd!2
	ldq	$27, __tls_get_addr($gp)	!literal!2
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsgd!2
	ldgp	$gp, 0($26)

	/* LD */
	lda	$16, sl1($gp)			!tlsldm!3
	ldq	$27, __tls_get_addr($gp)	!literal!3
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsldm!3
	ldgp	$gp, 0($26)
	lda	$1, sl1+1($0)			!dtprel

	/* LD with 4 variables */
	lda	$16, sh1($gp)			!tlsldm!4
	ldq	$27, __tls_get_addr($gp)	!literal!4
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsldm!4
	ldgp	$gp, 0($26)
	lda	$1, sh1($0)			!dtprel
	lda	$1, sh2+2($0)			!dtprel
	ldah	$1, sh3+3($0)			!dtprelhi
	lda	$1, sh3+3($1)			!dtprello
	ldq	$1, sh4+10($gp)			!gotdtprel
	addq	$1, $0, $1

	ldq	$26, 0($sp)
	addq	$sp, 16, $sp
	ret
	.end	fn2
