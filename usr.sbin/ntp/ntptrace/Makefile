#
# $FreeBSD: src/usr.sbin/ntp/ntptrace/Makefile,v 1.2 2000/01/01 23:58:01 roberto Exp $
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntptrace

PROG= ntptrace

SRCS= 	ntptrace.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

NOMAN= yes

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntptrace

.include <bsd.prog.mk>
