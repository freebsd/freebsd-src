#
# $FreeBSD: src/usr.sbin/xntpd/util/Makefile,v 1.9.2.1 1999/08/29 15:49:32 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= tickadj
MAN8= ${.CURDIR}/../doc/tickadj.8
CLEANFILES+= .version version.c

SRCS= tickadj.c version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion tickadj

.include <bsd.prog.mk>
