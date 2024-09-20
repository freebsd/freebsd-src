YPSERV=${SRCTOP}/usr.sbin/ypserv/common
.PATH: ${YPSERV}

PACKAGE=	yp
PROG=	ypwhich
SRCS=	yplib_host.c ypwhich.c

CFLAGS+=	-I${YPSERV} -I.

WARNS?=	2

.include <bsd.prog.mk>
