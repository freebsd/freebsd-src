LIB=	alias
SHLIB_MAJOR= 2
SHLIB_MINOR= 3
CFLAGS+=-Wall -I${.CURDIR}
SRCS=	alias.c alias_db.c alias_ftp.c alias_irc.c alias_util.c alias_old.c
MAN3=libalias.3 

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/alias.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
