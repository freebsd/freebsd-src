# $FreeBSD$

# Vendor sources and generated files
LDNSDIR= ${.CURDIR}/../../contrib/ldns

.PATH: ${LDNSDIR}/drill

PROG=	drill
SRCS=	drill.c drill_util.c error.c root.c work.c \
	chasetrace.c dnssec.c securetrace.c
CFLAGS+= -I${LDNSDIR}
NO_WERROR= true
MAN=	drill.1

DPADD+=	${LIBLDNS} ${LIBCRYPTO}
LDADD+=	-lldns -lcrypto
USEPRIVATELIB= ldns

.include <bsd.prog.mk>
