#	@(#)Makefile	8.2 (Berkeley) 4/4/94
# $FreeBSD$

PROG=	ftpd
MAN=	ftpd.8
SRCS=	ftpd.c ftpcmd.y logwtmp.c popen.c

CFLAGS+=-DSETPROCTITLE -DLOGIN_CAP -DVIRTUAL_HOSTING -Wall
CFLAGS+=-DINET6
CFLAGS+=-I${.CURDIR}
YFLAGS=

LDADD=	-lmd -lcrypt -lutil
DPADD=	${LIBMD} ${LIBCRYPT} ${LIBUTIL}

# XXX Kluge! Conversation mechanism needs to be fixed.
LDADD+=	-lopie
DPADD+=	${LIBOPIE}

LSDIR=	../../bin/ls
.PATH:	${.CURDIR}/${LSDIR}
SRCS+=	ls.c cmp.c print.c util.c lomac.c
CFLAGS+=-Dmain=ls_main -I${.CURDIR}/${LSDIR}

.if !defined(NOPAM)
CFLAGS+=-DUSE_PAM
DPADD+= ${LIBPAM}
LDADD+= ${MINUSLPAM}
.endif

.include <bsd.prog.mk>
