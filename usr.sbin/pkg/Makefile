# $FreeBSD$

PROG=	pkg
SRCS=	pkg.c dns_utils.c config.c
MAN=	pkg.7

CFLAGS+=-I${.CURDIR}/../../contrib/libyaml/include
.PATH:	${.CURDIR}/../../contrib/libyaml/include
DPADD=	${LIBARCHIVE} ${LIBELF} ${LIBFETCH} ${LIBYAML} ${LIBSBUF} ${LIBSSL} \
	${LIBCRYPTO}
LDADD=	-larchive -lelf -lfetch -lyaml -lsbuf -lssl -lcrypto
USEPRIVATELIB=	yaml

.include <bsd.prog.mk>
