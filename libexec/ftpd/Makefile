#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftpd
MAN8=	ftpd.8
SRCS=	ftpd.c ftpcmd.y logwtmp.c popen.c skey-stuff.c

CFLAGS+=-DSETPROCTITLE -DSKEY -DSTATS

LDADD=	-lskey -lmd -lcrypt -lutil
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}

CLEANFILES+=ftpcmd.tab.h

.include <bsd.prog.mk>
