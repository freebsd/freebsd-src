# $FreeBSD$

PROG=	apmd
MAN=	apmd.8
MANSUBDIR= /i386
SRCS=	apmd.c apmdlex.l apmdparse.y y.tab.h

WARNS?=	0

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
