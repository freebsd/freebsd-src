# $Id: Makefile,v 1.12 1998/05/04 20:09:02 bde Exp $

PROG = colldef
LFLAGS = -8 -i
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale -DCOLLATE_DEBUG
SRCS = parse.y scan.l y.tab.h
LDADD = -ll
DPADD = ${LIBL}
SUBDIR = data

.include <bsd.prog.mk>
