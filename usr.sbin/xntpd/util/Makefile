#
# $Id: Makefile,v 1.8 1998/03/07 09:46:12 bde Exp $
#

CFLAGS+= -I${.CURDIR}/../include

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

PROG= tickadj
MAN8= ${.CURDIR}/../doc/tickadj.8
CLEANFILES+= .version version.c

SRCS= tickadj.c version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion tickadj

.include <bsd.prog.mk>
