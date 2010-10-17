	.set nomacro
	ldq	$16, c($29)			!tlsgd!1
	ldq	$27, __tls_get_addr($29)	!literal!1
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsgd!1
	jsr	$26, ($27), __tls_get_addr	!lituse_jsr!1

	ldq	$16, d($29)			!tlsldm!2
	ldq	$27, __tls_get_addr($29)	!literal!2
	jsr	$26, ($27), __tls_get_addr	!lituse_jsr!2
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsldm!2

	ldq	$16, g($29)			!tlsgd!3
	ldq	$27, __tls_get_addr($29)	!literal!3
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsgd!3
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsgd!3
	
	ldq	$16, h($29)			!tlsldm!4
	ldq	$27, __tls_get_addr($29)	!literal!4
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsldm!4
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsldm!4

	ldq	$16, i($29)			!tlsgd!5
	ldq	$16, i($29)			!tlsgd!5

	ldq	$16, j($29)			!tlsldm!6
	ldq	$16, j($29)			!tlsldm!6

	ldq	$16, k($29)			!tlsgd!7
	ldq	$16, k($29)			!tlsldm!7

	ldq	$16, l($29)			!tlsldm!8
	ldq	$16, l($29)			!tlsgd!8
