#
# $FreeBSD: src/usr.sbin/audit/Makefile,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/audit

PROG=	audit
MAN=	audit.8

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
