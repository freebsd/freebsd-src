# $FreeBSD: src/gnu/lib/libreadline/history/Makefile,v 1.6.34.1 2009/04/15 03:14:26 kensmith Exp $

SUBDIR= doc

LIB=    history
MAN=    rlhistory.3

SRCS= $(HISTSRC) xmalloc.c

rlhistory.3: doc/history.3
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES+= rlhistory.3

.include <bsd.lib.mk>
