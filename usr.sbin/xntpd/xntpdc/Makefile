#
# $Id: Makefile,v 1.4 1996/06/24 04:26:21 jkh Exp $
#

CFLAGS+= -I${.CURDIR}/../include

.if exists(${.OBJDIR}/../lib)
LDADD+= -L${.OBJDIR}/../lib
DPADD+= ${.OBJDIR}/../lib/libntp.a
.else
LDADD+= -L${.CURDIR}/../lib
DPADD+= ${.CURDIR}/../lib/libntp.a
.endif

LDADD+= -lntp

PROG= xntpdc
MAN8= ${.CURDIR}/../doc/xntpdc.8
CLEANFILES+= .version version.c

SRCS= ntpdc.c ntpdc_ops.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion xntpdc

.include <bsd.prog.mk>
