# $FreeBSD: src/usr.bin/colldef/Makefile,v 1.16 1999/10/18 14:00:32 ache Exp $

PROG = colldef
LFLAGS = -8 -i
CFLAGS += -Wall
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale -DCOLLATE_DEBUG
SRCS = parse.y scan.l y.tab.h
LDADD = -ll
DPADD = ${LIBL}

.include <bsd.prog.mk>
