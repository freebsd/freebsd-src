#
# $Id: Makefile,v 1.1 1993/12/21 19:46:16 wollman Exp $
#

CFLAGS+= -I${.CURDIR}/../include
LDADD+= -L${.CURDIR}/../lib -lntp
DPADD+= -L${.CURDIR}/../lib/libntp.a

PROG= xntpdc
MAN8= ${.CURDIR}/../doc/xntpdc.8
CLEANFILES+= version.c

SRCS= ntpdc.c ntpdc_ops.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	${.CURDIR}/../scripts/mkversion xntpdc

.include <bsd.prog.mk>
