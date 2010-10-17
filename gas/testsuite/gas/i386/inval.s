	.text
# All the following should be illegal
	mov	(%dx),%al
	mov	(%eax,%esp,2),%al
	setae	%eax
	pushb	%ds
	popb	%ds
	pushb	%al
	popb	%al
	pushb	%ah
	popb	%ah
	pushb	%ax
	popb	%ax
	pushb	%eax
	popb	%eax
	movb	%ds,%ax
	movb	%ds,%eax
	movb	%ax,%ds
	movb	%eax,%ds
	movdb	%eax,%mm0
	movqb	0,%mm0
	ldsb	0,%eax
	setnew	0
	movdw	%eax,%mm0
	movqw	0,%mm0
	div	%cx,%al
	div	%cl,%ax
	div	%ecx,%al
	imul	10,%bx,%ecx
	imul	10,%bx,%al
	popab
	stil
	aaab
	cwdel
	cwdw
	callww	0
foo:	jaw	foo
	jcxzw	foo
	jecxzl	foo
	loopb	foo
	xlatw	%es:%bx
	xlatl	%es:%bx
	intl	2
	int3b
	hltb
	fstb	%st(0)
	fcompll	28(%ebp)
	fldlw	(%eax)
