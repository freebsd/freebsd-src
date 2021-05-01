# $FreeBSD$

# Vendor sources and generated files
LDNSDIR= ${SRCTOP}/contrib/ldns

.PATH: ${LDNSDIR}/drill

PROG=	drill
SRCS=	drill.c drill_util.c error.c root.c work.c \
	chasetrace.c dnssec.c securetrace.c
CFLAGS+= -I${LDNSDIR}
MK_WERROR=	no
MAN=	drill.1

LIBADD=	ldns crypto

.include <bsd.prog.mk>
