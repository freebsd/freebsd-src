#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# regex sources
.PATH: ${.CURDIR}/../libc/regex

CFLAGS+=-DPOSIX_MISTAKE

SRCS+=	regcomp.c regerror.c regexec.c regfree.c
