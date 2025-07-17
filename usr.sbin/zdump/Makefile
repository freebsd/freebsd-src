.PATH: ${SRCTOP}/contrib/tzcode

PROG=	zdump
MAN=	zdump.8
SRCS=	zdump.c

CFLAGS+= -I${SRCTOP}/contrib/tzcode -include tzconfig.h

.include <bsd.prog.mk>
