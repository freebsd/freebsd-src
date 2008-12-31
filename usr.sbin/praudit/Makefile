#
# $FreeBSD: src/usr.sbin/praudit/Makefile,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
