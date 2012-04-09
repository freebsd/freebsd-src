#
# $FreeBSD: src/usr.sbin/praudit/Makefile,v 1.1.12.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
