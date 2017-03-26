#
# $FreeBSD$
#

OPENBSMDIR=${SRCTOP}/contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/praudit

CFLAGS+=	-I${OPENBSMDIR}

PROG=	praudit
MAN=	praudit.1

WARNS?=	3

LIBADD=	bsm

.include <bsd.prog.mk>
