# $FreeBSD: src/usr.bin/mk_cmds/Makefile,v 1.9 1999/12/12 02:43:07 chris Exp $

PROG=	mk_cmds
SRCS=	mk_cmds.c options.c utils.c ct.y cmd_tbl.l y.tab.h
CFLAGS+= -I. -I${.CURDIR}/../../lib/libss -DIN_MK_CMDS
LFLAGS=	-l

LDADD+=	-ll
DPADD+=	${LIBL}

.include <bsd.prog.mk>
