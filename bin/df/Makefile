#	$Id: Makefile,v 1.6 1997/08/07 21:30:59 steve Exp $
#	@(#)Makefile	8.3 (Berkeley) 5/8/95

PROG=	df
SRCS=	df.c vfslist.c
BINGRP=	operator
BINMODE=2555

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
