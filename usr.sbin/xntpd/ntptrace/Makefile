#
# $Id: Makefile,v 1.3 1994/10/03 23:51:26 phk Exp $
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

PROG= ntptrace
MAN8= ${.CURDIR}/../doc/ntptrace.8
CLEANFILES+= .version version.c

SRCS= ntptrace.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion ntptrace

.include <bsd.prog.mk>
