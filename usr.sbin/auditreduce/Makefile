#
#

OPENBSMDIR=${SRCTOP}/contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditreduce

CFLAGS+=	-I${OPENBSMDIR}

PACKAGE=audit
PROG=	auditreduce
MAN=	auditreduce.1

LIBADD=	bsm

WARNS?=	3

.include <bsd.prog.mk>
