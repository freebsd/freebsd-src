#
# $Id: Makefile,v 1.1 1993/12/21 21:06:20 wollman Exp $
#
# Most of the programs in this directory are completely useless for the
# NTP configuration that we provide by default.
# We provide the `md5' program as a public service.

CFLAGS+= -I${.CURDIR}/../include

.if exists(${.CURDIR}/../lib/obj)
LDADD+= -L${.CURDIR}/../lib/obj
DPADD+= ${.CURDIR}/../lib/obj/libntp.a
.else
LDADD+= -L${.CURDIR}/../lib
DPADD+= ${.CURDIR}/../lib/libntp.a
.endif

LDADD+= -lntp

PROG= md5

SRCS= md5driver.c
NOMAN=

install:

.include <bsd.prog.mk>
