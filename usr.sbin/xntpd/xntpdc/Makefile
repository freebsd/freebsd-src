#
# $Id: Makefile,v 1.4.2.1 1998/03/09 14:46:28 jkh Exp $
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
