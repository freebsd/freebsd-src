# $FreeBSD$

PROG = colldef
LFLAGS = -8 -i
YFLAGS = -d
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale -DCOLLATE_DEBUG
SRCS = parse.c scan.c
LDADD = -ll
DPADD = ${LIBL}
CLEANFILES += lex.yy.c parse.c scan.c y.tab.[ch]
SUBDIR = data

.include <bsd.prog.mk>
