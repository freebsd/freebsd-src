#
# $FreeBSD: src/usr.sbin/praudit/Makefile,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
