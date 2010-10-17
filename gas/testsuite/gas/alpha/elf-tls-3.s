	.set nomacro
	ldq	$27, __tls_get_addr($29)	!literal!1
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsgd!1

	ldq	$27, __tls_get_addr($29)	!literal!2
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsldm!2

	ldq	$16, a($29)			!tlsgd!3
	ldq	$27, __tls_get_addr($29)	!literal!3
	ldq	$27, __tls_get_addr($29)	!literal!3
	
	ldq	$16, b($29)			!tlsldm!4
	ldq	$27, __tls_get_addr($29)	!literal!4
	ldq	$27, __tls_get_addr($29)	!literal!4

	ldq	$16, e($29)			!tlsgd!5
	ldq	$27, __tls_get_addr($29)	!literal!5
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsldm!5
	
	ldq	$16, f($29)			!tlsldm!6
	ldq	$27, __tls_get_addr($29)	!literal!6
	jsr	$26, ($27), __tls_get_addr	!lituse_tlsgd!6
