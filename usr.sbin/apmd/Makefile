# Makefile for apmd
# $Id: Makefile,v 1.1.3.2 1999/06/08 09:01:47 koie Exp $

PROG=	apmd
SHSRCS=	apmd.c apmdparse.y apmdlex.l
GENSRCS= 
GENHDRS= y.tab.h
SRCS= ${SHSRCS} ${GENSRCS} ${GENHDRS}

DPADD+= ${LIBL}
LDADD+= -ll

MAN8=	apmd.8

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR} -Wall #-DYY_STACK_USED
# for debug:
#CFLAGS+= -g -DDEBUG

test:
	./apmd -d -f etc/apmd.conf -n

.include <bsd.prog.mk>
