#
# $Id: Makefile,v 1.1 1993/12/21 20:16:18 wollman Exp $
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

PROG= tickadj
MAN8= ${.CURDIR}/../doc/tickadj.8
CLEANFILES+= .version version.c

SRCS= tickadj.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	${.CURDIR}/../scripts/mkversion tickadj

.include <bsd.prog.mk>
