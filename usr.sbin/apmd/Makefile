# Makefile for apmd
# $Id: Makefile,v 1.1.1.1 1999/07/10 17:39:35 iwasaki Exp $

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
