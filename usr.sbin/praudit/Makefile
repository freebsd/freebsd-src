#
# $FreeBSD: src/usr.sbin/praudit/Makefile,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
