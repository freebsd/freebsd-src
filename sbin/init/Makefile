#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	init
MAN=	init.8
MLINKS=	init.8 securelevel.8
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

beforeinstall:
.if exists(${DESTDIR}${BINDIR}/${PROG})
	-chflags noschg ${DESTDIR}${BINDIR}/${PROG}
	mv ${DESTDIR}${BINDIR}/${PROG} ${DESTDIR}${BINDIR}/${PROG}.bak
.endif

.include <bsd.prog.mk>
