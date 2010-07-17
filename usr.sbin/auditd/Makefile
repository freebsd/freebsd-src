#
# $FreeBSD: src/usr.sbin/auditd/Makefile,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditd

CFLAGS+=	-I${OPENBSMDIR}

PROG=	auditd
SRCS=	auditd.c audit_warn.c auditd_fbsd.c
MAN=	auditd.8

DPADD=	${LIBBSM} ${LIBAUDITD}
LDADD=	-lbsm -lauditd

.include <bsd.prog.mk>
