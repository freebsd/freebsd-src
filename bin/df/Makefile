#	@(#)Makefile	8.3 (Berkeley) 5/8/95
#	$Id: Makefile,v 1.4 1997/02/22 14:02:55 peter Exp $

PROG=	df
SRCS=	df.c vfslist.c
BINGRP=	operator
BINMODE=2555

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
