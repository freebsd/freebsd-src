#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.7 1998/09/19 22:42:09 obrien Exp $

PROG=	lprm
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin

.include <bsd.prog.mk>
