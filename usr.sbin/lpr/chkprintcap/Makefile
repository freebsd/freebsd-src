.PATH: ${.CURDIR:H}/common_source

PACKAGE=lp
PROG=	chkprintcap
MAN=	chkprintcap.8
SRCS=	chkprintcap.c skimprintcap.c

CFLAGS+= -I${.CURDIR:H}/common_source

LIBADD=	lpr

.include <bsd.prog.mk>
