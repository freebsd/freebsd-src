#
# $Id: Makefile,v 1.7 1997/02/22 16:14:43 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= xntpdc
MAN8= ${.CURDIR}/../doc/xntpdc.8
CLEANFILES+= .version version.c

SRCS= ntpdc.c ntpdc_ops.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion xntpdc

.include <bsd.prog.mk>
