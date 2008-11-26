# $FreeBSD: src/gnu/lib/libreadline/history/Makefile,v 1.6.30.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR= doc

LIB=    history
MAN=    rlhistory.3

SRCS= $(HISTSRC) xmalloc.c

rlhistory.3: doc/history.3
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES+= rlhistory.3

.include <bsd.lib.mk>
