# $FreeBSD$
# Author: Shteryana Shopova <syrinx@FreeBSD.org>

.include <bsd.own.mk>

.PATH: ${.CURDIR}

PROG=	bsnmpget

DPADD+=	${LIBBSNMP} ${LIBBSNMPTOOLS}
LDADD+=	-lbsnmp -lbsnmptools
CFLAGS+=	-I${.CURDIR}/../libbsnmptools
LDFLAGS+=	-L${LIBBSNMPTOOLSDIR}

.if ${MK_OPENSSL} != "no"
DPADD+= ${LIBCRYPTO}
LDADD+= -lcrypto
.endif

LINKS=  ${BINDIR}/bsnmpget ${BINDIR}/bsnmpwalk
LINKS+=  ${BINDIR}/bsnmpget ${BINDIR}/bsnmpset

MAN=	bsnmpget.1

MLINKS=		bsnmpget.1 bsnmpwalk.1
MLINKS+=	bsnmpget.1 bsnmpset.1

.include <bsd.prog.mk>
