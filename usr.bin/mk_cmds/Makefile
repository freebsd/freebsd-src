#	$Id: Makefile,v 1.5 1997/02/22 19:56:09 peter Exp $

PROG=	mk_cmds
SRCS=	mk_cmds.c options.c utils.c ct.y cmd_tbl.l
CFLAGS+= -I. -I${.CURDIR}/../../lib/libss -DIN_MK_CMDS
LFLAGS=	-l
NOMAN=		# XXX

LDADD+=	-ll
DPADD+=	${LIBL}

.include <bsd.prog.mk>
