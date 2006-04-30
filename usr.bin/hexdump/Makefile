#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	hexdump
SRCS=	conv.c display.c hexdump.c hexsyntax.c odsyntax.c parse.c
MAN=	hexdump.1 od.1
MLINKS=	hexdump.1 hd.1
LINKS=	${BINDIR}/hexdump ${BINDIR}/od
LINKS+=	${BINDIR}/hexdump ${BINDIR}/hd
WARNS?=	6

.include <bsd.prog.mk>
