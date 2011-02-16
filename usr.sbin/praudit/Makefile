#
# $FreeBSD: src/usr.sbin/praudit/Makefile,v 1.1.12.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
