	.abicalls
	.text

	/* These have obvious meanings, but we don't have currently defined
	   relocations for them.  */
	addiu	$4,$28,%dtprel(tlsvar)
	addiu	$4,$28,%tprel(tlsvar)
	addiu	$4,$28,%lo(%gottprel(tlsvar))
	addiu	$4,$28,%hi(%gottprel(tlsvar))
