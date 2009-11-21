#
# $FreeBSD: src/usr.sbin/pmccontrol/Makefile,v 1.2.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#

PROG=	pmccontrol
MAN=	pmccontrol.8

DPADD=	${LIBPMC}
LDADD=	-lpmc

WARNS?=	6

SRCS=	pmccontrol.c

.include <bsd.prog.mk>
