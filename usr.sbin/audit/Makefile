#
# $FreeBSD: src/usr.sbin/audit/Makefile,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/audit

PROG=	audit
MAN=	audit.8

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
