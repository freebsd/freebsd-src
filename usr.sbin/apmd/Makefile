# Makefile for apmd
# $FreeBSD: src/usr.sbin/apmd/Makefile,v 1.2 1999/08/28 01:15:24 peter Exp $

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
