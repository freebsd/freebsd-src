#
# $Id: Makefile,v 1.1 1993/12/21 19:50:40 wollman Exp $
#

CFLAGS+= -I${.CURDIR}/../include
LDADD+= -L${.CURDIR}/../lib -lntp
DPADD+= -L${.CURDIR}/../lib/libntp.a

PROG= xntpres
NOMAN=
CLEANFILES+= version.c

SRCS= xntpres.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	${.CURDIR}/../scripts/mkversion xntpres

.include <bsd.prog.mk>
