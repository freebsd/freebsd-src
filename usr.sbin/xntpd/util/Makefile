#
# $Id: Makefile,v 1.7 1997/02/22 16:14:37 peter Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= tickadj
MAN8= ${.CURDIR}/../doc/tickadj.8
CLEANFILES+= .version version.c

SRCS= tickadj.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion tickadj

.include <bsd.prog.mk>
