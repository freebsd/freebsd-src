#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftpd
MAN8=	ftpd.8
SRCS=	ftpd.c ftpcmd.c logwtmp.c popen.c skey-stuff.c

CFLAGS+=-DSETPROCTITLE -DSKEY -Wall

LDADD=	-lskey -lmd -lcrypt -lutil
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}

CLEANFILES+=ftpcmd.c y.tab.h

.if defined(KERBEROS)
SRCS+=	klogin.c
LDADD+=	-lkrb -ldes
DPADD+= ${LIBKRB} ${LIBDES}
CFLAGS+=-DKERBEROS
.endif

.include <bsd.prog.mk>
