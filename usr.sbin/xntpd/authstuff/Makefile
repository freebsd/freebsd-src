#
# $FreeBSD: src/usr.sbin/xntpd/authstuff/Makefile,v 1.6.2.1 1999/08/29 15:49:10 peter Exp $
#
# Most of the programs in this directory are completely useless for the
# NTP configuration that we provide by default.
# We provide the `md5' program as a public service.

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= md5

SRCS= md5driver.c
NOMAN=

install:

.include <bsd.prog.mk>
