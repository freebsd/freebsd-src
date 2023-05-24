# $FreeBSD$

# Vendor sources and generated files
LDNSDIR= ${SRCTOP}/contrib/ldns

.PATH: ${LDNSDIR}/drill

PROG=	drill
SRCS=	drill.c drill_util.c error.c root.c work.c \
	chasetrace.c dnssec.c securetrace.c
CFLAGS+= -I${LDNSDIR} -I${LDNSDIR}/ldns
CFLAGS+= -DLDNS_TRUST_ANCHOR_FILE='"/etc/unbound/root.key"'
MAN=	drill.1

LIBADD=	ldns crypto

.include <bsd.prog.mk>
