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

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
