# $FreeBSD: src/usr.sbin/apmd/Makefile,v 1.7 2001/07/20 06:19:37 obrien Exp $

PROG=	apmd
MAN=	apmd.8
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
