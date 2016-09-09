# $FreeBSD$

# Vendor sources and generated files
LDNSDIR= ${.CURDIR}/../../../contrib/ldns
UNBOUNDDIR= ${.CURDIR}/../../../contrib/unbound

.PATH: ${UNBOUNDDIR} ${UNBOUNDDIR}/smallapp ${UNBOUNDDIR}/util ${UNBOUNDDIR}/doc

PROG=	unbound-control
SRCS=	ub_event.c unbound-control.c worker_cb.c
CFLAGS+=	-I${UNBOUNDDIR} -I${LDNSDIR}
LIBADD=	unbound crypto ssl pthread
MAN=	unbound-control.8

.include <bsd.prog.mk>
