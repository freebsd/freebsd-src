# $FreeBSD: src/usr.bin/mk_cmds/Makefile,v 1.7.2.2 1999/12/12 03:04:10 chris Exp $

PROG=	mk_cmds
SRCS=	mk_cmds.c options.c utils.c ct.y cmd_tbl.l y.tab.h
CFLAGS+= -I. -I${.CURDIR}/../../lib/libss -DIN_MK_CMDS
LFLAGS=	-l

LDADD+=	-ll
DPADD+=	${LIBL}

.include <bsd.prog.mk>
