# $FreeBSD$

PROG=	pkg
SRCS=	pkg.c dns_utils.c config.c

NO_MAN=	yes
DPADD=	${LIBARCHIVE} ${LIBELF} ${LIBFETCH} ${LIBBSDYML} ${LIBSBUF}
LDADD=	-larchive -lelf -lfetch -lbsdyml -lsbuf

.include <bsd.prog.mk>
