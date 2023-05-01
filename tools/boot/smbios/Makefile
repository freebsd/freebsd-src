# $FreeBSD$

PROG=	smbios
MAN=
.PATH:	${SRCTOP}/stand/libsa
SRCS=	main.c
CFLAGS+= -I${.CURDIR} -I${SRCTOP}/stand/libsa

.include <bsd.prog.mk>

CFLAGS+= -Wno-cast-align
