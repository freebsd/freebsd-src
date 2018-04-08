# $FreeBSD$

BINDIR?=	/usr/bin

PROG=	prtblknos
MAN=

SRCS=	main.c prtblknos.c

LIBADD+=ufs

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>
