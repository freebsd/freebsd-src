#
# $FreeBSD$
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditreduce

PROG=	auditreduce
MAN=	auditreduce.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
