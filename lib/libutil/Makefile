#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	util
SHLIB_MAJOR= 2
SHLIB_MINOR= 1
CFLAGS+=-DLIBC_SCCS -I${.CURDIR} -I/sys
SRCS=	login.c login_tty.c logout.c logwtmp.c pty.c setproctitle.c
MAN3+=	setproctitle.3

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/libutil.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
