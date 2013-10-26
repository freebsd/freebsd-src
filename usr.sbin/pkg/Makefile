# $FreeBSD$

PROG=	pkg
SRCS=	pkg.c dns_utils.c config.c

NO_MAN=	yes
CFLAGS+=-I${.CURDIR}/../../contrib/libyaml/include
.PATH:	${.CURDIR}/../../contrib/libyaml/include
DPADD=	${LIBARCHIVE} ${LIBELF} ${LIBFETCH} ${LIBYAML} ${LIBSBUF} ${LIBSSL} \
	${LIBCRYPTO}
LDADD=	-larchive -lelf -lfetch -lyaml -lsbuf -lssl -lcrypto
USEPRIVATELIB=	yaml

.include <bsd.prog.mk>
