LIB=	alias
SHLIB_MAJOR= 2
SHLIB_MINOR= 5
CFLAGS+=-Wall -I${.CURDIR}
SRCS=	alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c \
	alias_nbt.c alias_old.c alias_util.c

MAN3=libalias.3 

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/alias.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
