#	@(#)Makefile	8.2 (Berkeley) 4/4/94
#	$Id$

PROG=	ftpd
MAN8=	ftpd.8
SRCS=	ftpd.c ftpcmd.c logwtmp.c popen.c skey-stuff.c

CFLAGS+=-DSETPROCTITLE -DSKEY -Wall

LDADD=	-lskey -lmd -lcrypt -lutil
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}

CLEANFILES+=ftpcmd.c y.tab.h

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_EBONES)
.PATH:  ${.CURDIR}/../../usr.bin/login
SRCS+=	klogin.c
LDADD+=	-lkrb -ldes
DPADD+= ${LIBKRB} ${LIBDES}
CFLAGS+=-DKERBEROS
DISTRIBUTION=	krb
.endif

.include <bsd.prog.mk>
