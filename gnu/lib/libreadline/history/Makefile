# $FreeBSD: src/gnu/lib/libreadline/history/Makefile,v 1.6.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $

SUBDIR= doc

LIB=    history
MAN=    rlhistory.3

SRCS= $(HISTSRC) xmalloc.c

rlhistory.3: doc/history.3
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES+= rlhistory.3

.include <bsd.lib.mk>
