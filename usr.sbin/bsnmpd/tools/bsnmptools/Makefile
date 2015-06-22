# $FreeBSD$
# Author: Shteryana Shopova <syrinx@FreeBSD.org>

.include <src.opts.mk>

.PATH: ${.CURDIR}

PROG=	bsnmpget

LIBADD=	bsnmp bsnmptools
CFLAGS+=	-I${.CURDIR}/../libbsnmptools

LINKS=  ${BINDIR}/bsnmpget ${BINDIR}/bsnmpwalk
LINKS+=  ${BINDIR}/bsnmpget ${BINDIR}/bsnmpset

MAN=	bsnmpget.1

MLINKS=		bsnmpget.1 bsnmpwalk.1
MLINKS+=	bsnmpget.1 bsnmpset.1

.include <bsd.prog.mk>
