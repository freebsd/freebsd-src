#
# $Id$
#
# Most of the programs in this directory are completely useless for the
# NTP configuration that we provide by default.
# We provide the `md5' program as a public service.

PROG= md5
LDADD+= -L${.CURDIR}/../lib -lntp
DPADD+= ${.CURDIR}/../lib/libntp.a
CFLAGS+= -I${.CURDIR}/../include

SRCS= md5driver.c
NOMAN=

install:

.include <bsd.prog.mk>
