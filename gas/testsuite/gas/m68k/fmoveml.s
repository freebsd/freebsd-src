# Test handling of the fmoveml and fmovemx instructions.
	.text
	.globl	foo
foo:
	fmoveml %fpcr,%a0@
	fmoveml %fpsr,%a0@
	fmoveml %fpiar,%a0@
	fmoveml %fpcr/%fpsr,%a0@
	fmoveml	%fpcr/%fpiar,%a0@
	fmoveml	%fpsr/%fpiar,%a0@
	fmoveml	%fpcr/%fpsr/%fpiar,%a0@
	fmoveml	%fpcr,%d0
	fmoveml	%fpsr,%d0
	fmoveml	%fpiar,%d0
	fmoveml	%fpiar,%a0
	fmoveml %a0@,%fpcr
	fmoveml %a0@,%fpsr
	fmoveml %a0@,%fpiar
	fmoveml %a0@,%fpsr/%fpcr
	fmoveml	%a0@,%fpiar/%fpcr
	fmoveml	%a0@,%fpiar/%fpsr
	fmoveml	%a0@,%fpsr/%fpiar/%fpcr
	fmoveml	%d0,%fpcr
	fmoveml	%d0,%fpsr
	fmoveml	%d0,%fpiar
	fmoveml	%a0,%fpiar
	fmoveml	&1,%fpcr
	fmoveml	&1,%fpsr
	fmoveml	&1,%fpiar
	fmoveml	&1,%fpcr/%fpsr
	fmoveml	&1,%fpcr/%fpiar
	fmoveml	&1,%fpsr/%fpiar
	fmoveml	&1,%fpiar/%fpsr/%fpcr

	fmovemx %fp1,%a0@
	fmovemx %fp4,%a0@
	fmovemx %fp7,%a0@
	fmovemx %fp1/%fp3,%a0@
	fmovemx	%fp1-%fp4,%a0@
	fmovemx	%fp0/%fp7,%a0@
	fmovemx	%fp0-%fp7,%a0@
	fmovemx %a0@,%fp0
	fmovemx %a0@,%fp1
	fmovemx %a0@,%fp7
	fmovemx %a0@,%fp0/%fp3
	fmovemx	%a0@,%fp0/%fp4
	fmovemx	%a0@,%fp2-%fp4
	fmovemx	%a0@,%fp1-%fp7
	fmovemx	&1,%a0@-
	fmovemx	&0xff,%a0@-
	fmovemx	&0x11,%a0@-
	fmovemx	%a0@+,&1
	fmovemx	%a0@+,&0xff
	fmovemx	%a0@+,&0x11
	fmovemx	%d0,%a0@-
	fmovemx	%a0@+,%d0
	fmovemx	&sym,%a0@-
	sym	=	0x22
