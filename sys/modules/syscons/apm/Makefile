# $FreeBSD$

KMOD=	apm_saver
SRCS= 	apm_saver.c

NOMAN=
CFLAGS+= -I${.CURDIR}/..

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
