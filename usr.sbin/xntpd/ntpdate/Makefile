#
# $FreeBSD: src/usr.sbin/xntpd/ntpdate/Makefile,v 1.10.2.1 1999/08/29 15:49:24 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= ntpdate
MAN8= ${.CURDIR}/../doc/ntpdate.8
CLEANFILES+= .version version.c

SRCS= ntpdate.c version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion ntpdate

.include <bsd.prog.mk>
