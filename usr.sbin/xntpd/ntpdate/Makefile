#
# $Id: Makefile,v 1.8 1997/02/22 16:14:26 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= ntpdate
MAN8= ${.CURDIR}/../doc/ntpdate.8
CLEANFILES+= .version version.c

SRCS= ntpdate.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion ntpdate

.include <bsd.prog.mk>
