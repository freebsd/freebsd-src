#	@(#)Makefile	8.2 (Berkeley) 4/4/94
# $FreeBSD: src/libexec/ftpd/Makefile,v 1.33.2.5 2002/03/28 18:11:37 joe Exp $

PROG=	ftpd
MAN=	ftpd.8
SRCS=	ftpd.c ftpcmd.y logwtmp.c popen.c skey-stuff.c

CFLAGS+=-DSETPROCTITLE -DSKEY -DLOGIN_CAP -DVIRTUAL_HOSTING -Wall
CFLAGS+=-DINET6
YFLAGS=

LDADD=	-lskey -lmd -lcrypt -lutil
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}

LSDIR=	../../bin/ls
.PATH:	${.CURDIR}/${LSDIR}
SRCS+=	ls.c cmp.c print.c util.c
CFLAGS+=-Dmain=ls_main -I${.CURDIR}/${LSDIR}

DPADD+=	${LIBM}
LDADD+=	-lm

.if defined(NOPAM)
CFLAGS+=-DNOPAM
.else
DPADD+= ${LIBPAM}
LDADD+= ${MINUSLPAM}
.endif

.include <bsd.prog.mk>
