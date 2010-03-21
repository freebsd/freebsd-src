#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/dd/Makefile,v 1.19.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	dd
SRCS=	args.c conv.c conv_tab.c dd.c misc.c position.c

#
# Test the character conversion functions.  We have to be explicit about
# which LC_LANG we use because the definition of upper and lower case
# depends on it.
#

CLEANFILES=	gen

test: ${PROG} gen
.for conv in ascii ebcdic ibm oldascii oldebcdic oldibm \
    pareven parnone parodd parset \
    swab lcase ucase
	@${ECHO} testing conv=${conv}
	@./gen | \
	    LC_ALL=en_US.US-ASCII ./dd conv=${conv} 2>/dev/null | \
	    LC_ALL=en_US.US-ASCII hexdump -C | \
	    diff -I FreeBSD - ${.CURDIR}/ref.${conv}
.endfor
	@rm -f gen

.include <bsd.prog.mk>
