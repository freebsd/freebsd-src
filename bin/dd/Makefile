#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	dd
SRCS=	args.c conv.c conv_tab.c dd.c misc.c position.c

MAINTAINER=	green@FreeBSD.org

.include <bsd.prog.mk>

CLEANFILES+= a.out
#
# Test the character conversion functions.  We have to be explicit about
# which LC_LANG we use because the definition of upper and lower case
# depends on it.
test:	${PROG}
	cc ${.CURDIR}/gen.c
.for conv in ascii ebcdic ibm oldascii oldebcdic oldibm \
    pareven parnone parodd parset \
    swab lcase ucase
	@echo testing conv=${conv}
	@./a.out | \
	  /usr/bin/env -i LC_CTYPE=en_US.US-ASCII ./dd conv=${conv} 2>/dev/null | \
	  /usr/bin/env -i LC_CTYPE=en_US.US-ASCII hexdump -C | \
	  diff -IFreeBSD - ${.CURDIR}/ref.${conv}
.endfor
