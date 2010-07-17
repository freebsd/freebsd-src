#
# $FreeBSD: src/usr.sbin/auditreduce/Makefile,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditreduce

CFLAGS+=	-I${OPENBSMDIR}

PROG=	auditreduce
MAN=	auditreduce.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
