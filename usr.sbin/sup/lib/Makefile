# $Id: Makefile,v 1.3 1996/01/04 16:10:36 peter Exp $

LIB=	sup
SRCS=	scm.c scmio.c stree.c log.c supmsg.c netcrypt.c atoo.c errmsg.c \
	expand.c ffilecopy.c filecopy.c nxtarg.c path.c quit.c run.c \
	salloc.c skipto.c scan.c

CFLAGS+=	-DVAR_TMP -DHAS_DAEMON -I${.CURDIR}
NOPROFILE=
CLEANFILES+=	netcrypt.c

netcrypt.c:	netcryptvoid.c
	ln -s ${.CURDIR}/netcryptvoid.c netcrypt.c

install:

.include <bsd.lib.mk>
