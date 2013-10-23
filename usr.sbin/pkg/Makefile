# $FreeBSD$

PROG=	pkg
SRCS=	pkg.c dns_utils.c config.c

NO_MAN=	yes
CFLAGS+=-I${.CURDIR}/../../contrib/libyaml/include
.PATH:	${.CURDIR}/../../contrib/libyaml/include
DPADD=	${LIBARCHIVE} ${LIBELF} ${LIBFETCH} ${LIBYAML} ${LIBSBUF}
LDADD=	-larchive -lelf -lfetch -lyaml -lsbuf
USEPRIVATELIB=	yaml

.include <bsd.prog.mk>
