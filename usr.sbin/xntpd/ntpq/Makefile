#
# $Id: Makefile,v 1.8 1997/02/22 16:14:28 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= ntpq
MAN8= ${.CURDIR}/../doc/ntpq.8
CLEANFILES+= .version version.c
BINDIR= /usr/bin

SRCS= ntpq.c ntpq_ops.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion ntpq

.include <bsd.prog.mk>
