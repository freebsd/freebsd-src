# $Id: Makefile,v 1.14 1998/12/06 22:58:17 archie Exp $

PROG = colldef
LFLAGS = -8 -i
CFLAGS += -Wall
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale -DCOLLATE_DEBUG
SRCS = parse.y scan.l y.tab.h
LDADD = -ll
DPADD = ${LIBL}
SUBDIR = data

.include <bsd.prog.mk>
