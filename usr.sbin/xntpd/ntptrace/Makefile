#
# $FreeBSD: src/usr.sbin/xntpd/ntptrace/Makefile,v 1.9.2.1 1999/08/29 15:49:26 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= ntptrace
MAN8= ${.CURDIR}/../doc/ntptrace.8
CLEANFILES+= .version version.c

SRCS= ntptrace.c version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion ntptrace

.include <bsd.prog.mk>
