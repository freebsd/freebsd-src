#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.7 1998/09/19 22:42:09 obrien Exp $

PROG=	lpr
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
MAN1=	lpr.1
MAN5=	printcap.5
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
