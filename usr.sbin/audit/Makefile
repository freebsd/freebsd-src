#
# $FreeBSD: src/usr.sbin/audit/Makefile,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/audit

PROG=	audit
MAN=	audit.8

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
