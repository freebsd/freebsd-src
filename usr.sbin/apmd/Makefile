# $FreeBSD: src/usr.sbin/apmd/Makefile,v 1.8.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	apmd
MAN=	apmd.8
MANSUBDIR= /i386
SRCS=	apmd.c apmdlex.l apmdparse.y y.tab.h

DPADD=	${LIBL}
LDADD=	-ll

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR} #-DYY_STACK_USED
# for debug:
#CFLAGS+= -g -DDEBUG

CLEANFILES= y.output

test:
	./apmd -d -f etc/apmd.conf -n

.include <bsd.prog.mk>
