# $FreeBSD$

PROG=	mk_cmds
SRCS=	mk_cmds.c options.c utils.c ct.y cmd_tbl.l y.tab.h
CFLAGS+= -I. -I${.CURDIR}/../../lib/libss -DIN_MK_CMDS
LFLAGS=	-l

LDADD+=	-ll
DPADD+=	${LIBL}

.include <bsd.prog.mk>
