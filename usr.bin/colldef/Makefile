# $Id: Makefile,v 1.6 1995/11/03 14:53:27 ache Exp $

PROG = colldef
LFLAGS = -8 -i
YFLAGS = -d
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale
SRCS = parse.c scan.c
LDADD = -ll
DPADD = ${LIBL}
CLEANFILES += parse.c scan.c parse.tab.h
SUBDIR = data

.include <bsd.prog.mk>
