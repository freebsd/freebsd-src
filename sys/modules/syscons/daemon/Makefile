# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/syscons/daemon

KMOD=	daemon_saver
SRCS=	daemon_saver.c

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
