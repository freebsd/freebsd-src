# $FreeBSD: src/gnu/lib/libreadline/history/Makefile,v 1.6.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

SUBDIR= doc

LIB=    history
MAN=    rlhistory.3

SRCS= $(HISTSRC) xmalloc.c

rlhistory.3: doc/history.3
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES+= rlhistory.3

.include <bsd.lib.mk>
