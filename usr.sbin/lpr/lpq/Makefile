#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.4 1997/12/16 17:53:28 bde Exp $

PROG=	lpq
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

BINOWN=	root
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin

.include <bsd.prog.mk>
