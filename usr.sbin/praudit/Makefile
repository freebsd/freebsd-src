#
# $FreeBSD: src/usr.sbin/praudit/Makefile,v 1.1.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
