# $FreeBSD$

.PATH: ${.CURDIR}/../tablegen

PROG=	refgen
NO_MAN=	yes
SRCS=	tablegen.c

WARNS?=	3
CFLAGS+= -L/usr/local/lib -I/usr/local/include
LDADD=	-liconv
DPADD=  /usr/local/lib/libiconv.a

.include <bsd.prog.mk>
