#
# $Id: Makefile,v 1.3 1994/10/03 23:51:34 phk Exp $
#

CFLAGS+= -I${.CURDIR}/../include

.if exists(${.OBJDIR}/../lib)
LDADD+= -L${.OBJDIR}/../lib
DPADD+= -L${.OBJDIR}/../lib/libntp.a
.else
LDADD+= -L${.CURDIR}/../lib
DPADD+= -L${.CURDIR}/../lib/libntp.a
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
