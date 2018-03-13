# $FreeBSD$

PACKAGE=runtime
PROG=	nvmecontrol
SRCS=	nvmecontrol.c devlist.c firmware.c format.c identify.c logpage.c \
	perftest.c reset.c nvme_util.c power.c util.c wdc.c
MAN=	nvmecontrol.8

.PATH:	${SRCTOP}/sys/dev/nvme

.include <bsd.prog.mk>
