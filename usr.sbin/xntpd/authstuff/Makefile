#
# $Id: Makefile,v 1.2 1993/12/22 11:32:10 rgrimes Exp $
#
# Most of the programs in this directory are completely useless for the
# NTP configuration that we provide by default.
# We provide the `md5' program as a public service.

CFLAGS+= -I${.CURDIR}/../include

.if exists(${.OBJDIR}/../lib)
LDADD+= -L${.OBJDIR}/../lib
DPADD+= ${.OBJDIR}/../lib/libntp.a
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
