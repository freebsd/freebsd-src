# $FreeBSD: src/usr.bin/mk_cmds/Makefile,v 1.7.2.1 1999/08/29 15:30:59 peter Exp $

PROG=	mk_cmds
SRCS=	mk_cmds.c options.c utils.c ct.y cmd_tbl.l y.tab.h
CFLAGS+= -I. -I${.CURDIR}/../../lib/libss -DIN_MK_CMDS
LFLAGS=	-l
NOMAN=		# XXX

LDADD+=	-ll
DPADD+=	${LIBL}

.include <bsd.prog.mk>
