#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/regex/Makefile.inc,v 1.8.14.1 2010/12/21 17:10:29 kensmith Exp $

# regex sources
.PATH: ${.CURDIR}/regex

CFLAGS+=-DPOSIX_MISTAKE

SRCS+=	regcomp.c regerror.c regexec.c regfree.c

SYM_MAPS+=${.CURDIR}/regex/Symbol.map

MAN+=	regex.3
MAN+=	re_format.7

MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3
MLINKS+=regexec.3 regfree.3
