#	@(#)Makefile	5.11 (Berkeley) 9/6/90

# regex sources
.PATH: ${.CURDIR}/regex

SRCS+=	regfree.c regcomp.c regerror.c regexec.c
CFLAGS+=-I${.CURDIR}/regex -DPOSIX_MISTAKE

MAN3+= regex/regex.3
MAN7+= regex/regex.7

MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3 
MLINK+= regex.3 regfree.3
