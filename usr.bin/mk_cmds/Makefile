#	$Id: Makefile,v 1.1 1995/01/19 22:27:57 wollman Exp $

PROG=	mk_cmds
#
# NB: ct.c must come before cmd_tbl.c so that y.tab.h will be generated.
#
SRCS=	mk_cmds.c options.c utils.c ct.c cmd_tbl.c
CFLAGS+= -I. -I${.CURDIR}/../../lib/libss -DIN_MK_CMDS
LFLAGS=	-l
CLEANFILES+=	y.tab.c y.tab.h lex.yy.c cmd_tbl.c ct.c
NOMAN=		# XXX

LDADD+=	-ll
DPADD+=	${LIBL}

.include <bsd.prog.mk>
