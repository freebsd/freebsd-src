# $FreeBSD$

PROG = colldef
LFLAGS = -8 -i
CFLAGS += -Wall
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale -DCOLLATE_DEBUG
SRCS = parse.y scan.l y.tab.h
LDADD = -ll
DPADD = ${LIBL}

.include <bsd.prog.mk>
