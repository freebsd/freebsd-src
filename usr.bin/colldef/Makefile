# $Id: Makefile,v 1.11 1997/02/22 19:54:30 peter Exp $

PROG = colldef
LFLAGS = -8 -i
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale -DCOLLATE_DEBUG
SRCS = parse.y scan.l
LDADD = -ll
DPADD = ${LIBL}
SUBDIR = data

.include <bsd.prog.mk>
