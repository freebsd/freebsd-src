# $FreeBSD: src/usr.sbin/apmd/Makefile,v 1.2.2.2 2001/04/25 12:09:18 ru Exp $

PROG=	apmd
SRCS=	apmd.c apmdlex.l apmdparse.y y.tab.h

DPADD=	${LIBL}
LDADD=	-ll

MAN=	apmd.8

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR} -Wall #-DYY_STACK_USED
# for debug:
#CFLAGS+= -g -DDEBUG

CLEANFILES= y.output

test:
	./apmd -d -f etc/apmd.conf -n

.include <bsd.prog.mk>
