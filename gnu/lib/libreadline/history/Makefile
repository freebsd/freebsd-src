# $FreeBSD$

SUBDIR= doc

LIB=    history
MAN=    rlhistory.3

SRCS= $(HISTSRC) xmalloc.c

rlhistory.3: history.3
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.lib.mk>
