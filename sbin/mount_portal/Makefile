#	From: @(#)Makefile	8.3 (Berkeley) 3/27/94
#	$Id: Makefile,v 1.8 1998/01/20 10:40:04 bde Exp $

PROG=	mount_portal
SRCS=	mount_portal.c activate.c conf.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_tcp.c pt_tcplisten.c
MAN8=	mount_portal.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
