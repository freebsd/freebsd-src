# $FreeBSD$

PROG=	colldef
SRCS=	parse.y scan.l y.tab.h
WARNS?=	2
NO_WERROR=yes
LFLAGS=	-8 -i
CFLAGS+=-I${.OBJDIR} -I${.CURDIR} -I${.CURDIR}/../../lib/libc/locale -DCOLLATE_DEBUG -DYY_NO_UNPUT
LDADD=	-ll
DPADD=	${LIBL}

.include <bsd.prog.mk>
