#
# $Id: Makefile,v 1.3 1993/12/22 11:32:56 rgrimes Exp $
#

CFLAGS+= -I${.CURDIR}/../include

.if exists(${.CURDIR}/../lib/obj)
LDADD+= -L${.CURDIR}/../lib/obj
DPADD+= -L${.CURDIR}/../lib/obj/libntp.a
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
	${.CURDIR}/../scripts/mkversion ntpdate

.include <bsd.prog.mk>
