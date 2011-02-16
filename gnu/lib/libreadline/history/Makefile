# $FreeBSD: src/gnu/lib/libreadline/history/Makefile,v 1.6.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

SUBDIR= doc

LIB=    history
MAN=    rlhistory.3

SRCS= $(HISTSRC) xmalloc.c

rlhistory.3: doc/history.3
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES+= rlhistory.3

.include <bsd.lib.mk>
