#	From: @(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_portal/Makefile,v 1.9.2.1 1999/08/29 15:14:31 peter Exp $

PROG=	mount_portal
SRCS=	mount_portal.c activate.c conf.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_tcp.c pt_tcplisten.c
MAN8=	mount_portal.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
