#
# $Id: Makefile,v 1.9 1998/03/19 15:12:05 bde Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	${LIBNTP} -ledit -ltermcap

PROG= xntpdc
MAN8= ${.CURDIR}/../doc/xntpdc.8
CLEANFILES+= .version version.c

SRCS= ntpdc.c ntpdc_ops.c version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion xntpdc

.include <bsd.prog.mk>
