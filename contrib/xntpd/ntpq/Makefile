#
# $Id: Makefile,v 1.3 1993/12/22 11:33:07 rgrimes Exp $
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

PROG= ntpq
MAN8= ${.CURDIR}/../doc/ntpq.8
CLEANFILES+= .version version.c
BINDIR= /usr/bin

SRCS= ntpq.c ntpq_ops.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	${.CURDIR}/../scripts/mkversion ntpq

.include <bsd.prog.mk>
