# $FreeBSD$

PROG=	pkg
SRCS=	pkg.c dns_utils.c

NO_MAN=	yes
DPADD=	${LIBARCHIVE} ${LIBELF} ${LIBFETCH}
LDADD=	-larchive -lelf -lfetch

.include <bsd.prog.mk>
