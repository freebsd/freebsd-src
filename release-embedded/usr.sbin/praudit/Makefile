#
# $FreeBSD$
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

PROG=	praudit
MAN=	praudit.1

WARNS?=	3

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
