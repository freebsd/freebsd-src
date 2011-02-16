#
# $FreeBSD: src/usr.sbin/auditd/Makefile,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditd

PROG=	auditd
SRCS=	auditd.c audit_warn.c
MAN=	auditd.8

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
