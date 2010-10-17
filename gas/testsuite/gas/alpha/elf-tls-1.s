	.set nomacro
	ldq	$27, __tls_get_addr($29)	!literal!1
	ldq	$16, a($29)			!tlsgd!1
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsgd!1

	jsr	$26, ($27), __tls_get_addr	!lituse_tlsldm!2
	ldq	$27, __tls_get_addr($29)	!literal!2
	ldq	$16, b($29)			!tlsldm!2

	ldq	$16, c($29)			!tlsgd
	ldq	$16, d($29)			!tlsldm

	ldq	$16, e($29)			!tlsgd!3
	ldq	$16, f($29)			!tlsldm!4

	ldq	$16, g($29)			!gotdtprel
	ldah	$16, h($31)			!dtprelhi
	lda	$16, i($16)			!dtprello
	lda	$16, j($31)			!dtprel

	ldq	$16, k($29)			!gottprel
	ldah	$16, l($31)			!tprelhi
	lda	$16, m($16)			!tprello
	lda	$16, n($31)			!tprel
