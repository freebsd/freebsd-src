# $FreeBSD$

MAINTAINER=	des@freebsd.org
PROG=		fetch
CFLAGS+=	-Wall -Wwrite-strings -Wmissing-prototypes
SRCS=		file.c ftp.c http.c main.c util.c uri.c
DPADD=		${LIBFTPIO} ${LIBMD}
LDADD=		-lftpio -lmd

.include <bsd.prog.mk>
