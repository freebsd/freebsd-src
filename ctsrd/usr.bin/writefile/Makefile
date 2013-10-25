# $FreeBSD$

PROG=	writefile

SRCS=	writefile.c eav.c

MAN=

CFLAGS+=	-DMD5_SUPPORT
LDADD+=		-lbz2 -lmd -lutil

.include <bsd.prog.mk>
