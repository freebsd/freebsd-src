#	@(#)Makefile	8.2 (Berkeley) 4/4/94
# $FreeBSD$

PROG=	ftpd
MAN8=	ftpd.8
SRCS=	ftpd.c ftpcmd.y logwtmp.c popen.c skey-stuff.c

CFLAGS+=-DSETPROCTITLE -DSKEY -DLOGIN_CAP -DVIRTUAL_HOSTING -Wall
CFLAGS+=-DINET6
CFLAGS+=-I${.CURDIR}
YFLAGS=

LDADD=	-lskey -lmd -lcrypt -lutil
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}

LSDIR=	../../bin/ls
.PATH:	${.CURDIR}/${LSDIR}
SRCS+=	ls.c cmp.c print.c util.c
CFLAGS+=-Dmain=ls_main -I${.CURDIR}/${LSDIR}

.if defined(NOPAM)
CFLAGS+=-DNOPAM
.else
DPADD+= ${LIBPAM}
LDADD+= ${MINUSLPAM}
.endif

.include <bsd.prog.mk>
