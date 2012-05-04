# $FreeBSD$

PROG=	pkg
SRC=	pkg.c

NO_MAN=	yes
DPADD=	${LIBARCHIVE} ${LIBELF} ${LIBFETCH}
LDADD=	-larchive -lelf -lfetch

.include <bsd.prog.mk>
