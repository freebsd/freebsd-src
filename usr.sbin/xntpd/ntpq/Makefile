#
# $Id$
#

CFLAGS+= -I${.CURDIR}/../include
LDADD+= -L${.CURDIR}/../lib -lntp
DPADD+= -L${.CURDIR}/../lib/libntp.a

PROG= ntpq
MAN8= ${.CURDIR}/../doc/ntpq.8
CLEANFILES+= version.o

SRCS= ntpq.c ntpq_ops.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	${.CURDIR}/../scripts/mkversion ntpq

.include <bsd.prog.mk>
