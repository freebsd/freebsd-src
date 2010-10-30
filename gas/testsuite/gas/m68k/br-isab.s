foo:	nop
	bsr.l foo
	jbra foo
	jbra bar
	jbsr foo
	jbsr bar
	nop
