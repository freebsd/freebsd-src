#	From: @(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_portal/Makefile,v 1.10 1999/08/28 00:13:34 peter Exp $

PROG=	mount_portal
SRCS=	mount_portal.c activate.c conf.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_tcp.c pt_tcplisten.c
MAN8=	mount_portal.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
