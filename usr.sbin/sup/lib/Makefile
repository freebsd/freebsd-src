# $FreeBSD$

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
