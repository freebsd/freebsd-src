	.set nomacro
	extbl	$3, $2, $3	! lituse_bytoff  !   1
	ldq	$2, a($29)	!literal!1
	ldq	$4, b($29)	!literal!2
	ldq_u	$3, 0($2)	!lituse_base!1
	ldq	$27, f($29)	!literal!5
	jsr	$26, ($27), f	!lituse_jsr!5

	lda	$0, c($29)	!gprel
	ldah	$1, d($29)	!gprelhigh
	lda	$1, e($1)	!gprellow

	ldah	$29, 0($26)	!gpdisp!3
	lda	$29, 0($29)	!gpdisp!4
	lda	$29, 0($29)	!gpdisp!3
	ldah	$29, 0($26)	!gpdisp!4
