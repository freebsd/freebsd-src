#
# $Id$
#

CFLAGS+= -I${.CURDIR}/../include
LDADD+= -L${.CURDIR}/../lib -lntp
DPADD+= -L${.CURDIR}/../lib/libntp.a

PROG= ntpdate
MAN8= ${.CURDIR}/../doc/ntpdate.8
CLEANFILES+= version.o

SRCS= ntpdate.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	${.CURDIR}/../scripts/mkversion ntpdate

.include <bsd.prog.mk>
