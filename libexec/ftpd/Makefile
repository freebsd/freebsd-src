#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftpd
MAN8=	ftpd.8
SRCS=	ftpd.c ftpcmd.c logwtmp.c popen.c skey-stuff.c

CFLAGS+=-DSETPROCTITLE -DSKEY -DSTATS

LDADD=	-lskey -lmd -lcrypt
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT} 

CLEANFILES+=ftpcmd.c y.tab.h

.include <bsd.prog.mk>
