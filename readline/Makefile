#	$NetBSD: Makefile,v 1.4 2001/05/16 07:09:26 enami Exp $	

.PATH:	${CURDIR}/..

INCS=		readline.h
INCSDIR=	/usr/include/readline
SYMLINKS=	readline.h ${INCSDIR}/history.h

MKOBJ=	no

incinstall:: linksinstall

.include <bsd.prog.mk>
