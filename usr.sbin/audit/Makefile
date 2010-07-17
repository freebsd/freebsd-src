#
# $FreeBSD: src/usr.sbin/audit/Makefile,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/audit

CFLAGS+=	-I${OPENBSMDIR}

PROG=	audit
MAN=	audit.8

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
