#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftpd
MAN8=	ftpd.8
SRCS=	ftpd.c ftpcmd.c logwtmp.c popen.c skey-stuff.c

CFLAGS+=-DSETPROCTITLE -DSKEY -DSTATS
CFLAGS+=-DFTP_DATA_BOTTOM=40000 -DFTP_DATA_TOP=44999

LDADD=	-lskey -lmd -lcrypt -lutil
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}

CLEANFILES+=ftpcmd.c y.tab.h

.include <bsd.prog.mk>
