#
# $Id: Makefile,v 1.1 1993/12/21 19:50:40 wollman Exp $
#

CFLAGS+= -I${.CURDIR}/../include
LDADD+= -L${.CURDIR}/../lib -lntp
DPADD+= -L${.CURDIR}/../lib/libntp.a

PROG= tickadj
MAN8= ${.CURDIR}/../doc/tickadj.8
CLEANFILES+= version.c

SRCS= tickadj.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	${.CURDIR}/../scripts/mkversion tickadj

.include <bsd.prog.mk>
