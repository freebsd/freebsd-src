# $FreeBSD: src/gnu/lib/libreadline/history/Makefile,v 1.6 2001/04/21 22:05:30 ache Exp $

SUBDIR= doc

LIB=    history
MAN=    rlhistory.3

SRCS= $(HISTSRC) xmalloc.c

rlhistory.3: doc/history.3
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES+= rlhistory.3

.include <bsd.lib.mk>
