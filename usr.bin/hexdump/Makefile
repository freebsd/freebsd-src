#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/hexdump/Makefile,v 1.3.8.1 2001/04/25 11:29:22 ru Exp $

PROG=	hexdump
SRCS=	conv.c display.c hexdump.c hexsyntax.c odsyntax.c parse.c
MAN=	hexdump.1 od.1
MLINKS=	hexdump.1 hd.1
LINKS=	${BINDIR}/hexdump ${BINDIR}/od
LINKS+=	${BINDIR}/hexdump ${BINDIR}/hd

.include <bsd.prog.mk>
