# $FreeBSD: src/usr.bin/colldef/Makefile,v 1.14.2.1 1999/08/29 15:26:20 peter Exp $

PROG = colldef
LFLAGS = -8 -i
CFLAGS += -Wall
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale -DCOLLATE_DEBUG
SRCS = parse.y scan.l y.tab.h
LDADD = -ll
DPADD = ${LIBL}
SUBDIR = data

.include <bsd.prog.mk>
