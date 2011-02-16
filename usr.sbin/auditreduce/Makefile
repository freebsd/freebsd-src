#
# $FreeBSD: src/usr.sbin/auditreduce/Makefile,v 1.2.14.1 2010/12/21 17:10:29 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditreduce

CFLAGS+=	-I${OPENBSMDIR}

PROG=	auditreduce
MAN=	auditreduce.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
