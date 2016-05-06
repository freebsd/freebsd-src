# $FreeBSD$

PACKAGE=runtime
PROG=	nvmecontrol
SRCS=	nvmecontrol.c devlist.c firmware.c identify.c logpage.c	\
	perftest.c reset.c nvme_util.c power.c
MAN=	nvmecontrol.8

.PATH:	${.CURDIR}/../../sys/dev/nvme

.include <bsd.prog.mk>
