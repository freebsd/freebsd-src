#
# $FreeBSD: src/usr.sbin/praudit/Makefile,v 1.1.2.1 2006/09/02 17:02:01 rwatson Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
