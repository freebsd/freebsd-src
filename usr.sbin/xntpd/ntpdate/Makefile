#
# $Id: Makefile,v 1.4 1994/10/03 23:51:17 phk Exp $
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

PROG= ntpdate
MAN8= ${.CURDIR}/../doc/ntpdate.8
CLEANFILES+= .version version.c

SRCS= ntpdate.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion ntpdate

.include <bsd.prog.mk>
