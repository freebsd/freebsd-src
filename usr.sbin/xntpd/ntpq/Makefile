#
# $Id: Makefile,v 1.9 1998/03/07 09:46:08 bde Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= ntpq
MAN8= ${.CURDIR}/../doc/ntpq.8
CLEANFILES+= .version version.c
BINDIR= /usr/bin

SRCS= ntpq.c ntpq_ops.c version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion ntpq

.include <bsd.prog.mk>
