#
# $Id: Makefile,v 1.7 1997/02/22 16:14:30 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= ntptrace
MAN8= ${.CURDIR}/../doc/ntptrace.8
CLEANFILES+= .version version.c

SRCS= ntptrace.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion ntptrace

.include <bsd.prog.mk>
