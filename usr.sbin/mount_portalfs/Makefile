#	From: @(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/usr.sbin/mount_portalfs/Makefile,v 1.17 2004/02/23 20:21:21 johan Exp $

PROG=	mount_portalfs
SRCS=	mount_portalfs.c activate.c conf.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_tcp.c pt_tcplisten.c
MAN=	mount_portalfs.8

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+=-I${MOUNT}
WARNS?=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>
