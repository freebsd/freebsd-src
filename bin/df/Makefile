#	$Id: Makefile,v 1.5 1997/03/13 17:29:08 bde Exp $
#	@(#)Makefile	8.3 (Berkeley) 5/8/95

PROG=	df
SRCS=	df.c vfslist.c
BINGRP=	operator
BINMODE=2555

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT} -D_NEW_VFSCONF
.PATH:	${MOUNT}

.include <bsd.prog.mk>
