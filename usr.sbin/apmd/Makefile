# $FreeBSD$

PROG=	apmd
MAN=	apmd.8
MANSUBDIR= /i386
SRCS=	apmd.c apmdlex.l apmdparse.y y.tab.h

WARNS?=	3

DPADD=	${LIBL}
LDADD=	-ll

CFLAGS+= -I${.CURDIR}

test:
	./apmd -d -f etc/apmd.conf -n

.include <bsd.prog.mk>
