#	@(#)Makefile	8.2 (Berkeley) 4/4/94
#	$Id: Makefile,v 1.17 1996/08/09 09:02:24 markm Exp $

PROG=	ftpd
MAN8=	ftpd.8
SRCS=	ftpd.c ftpcmd.c logwtmp.c popen.c skey-stuff.c

CFLAGS+=-DSETPROCTITLE -DSKEY -Wall

LDADD=	-lskey -lmd -lcrypt -lutil
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}

CLEANFILES+=ftpcmd.c y.tab.h

.ifdef FTPD_INTERNAL_LS
LSDIR=	../../bin/ls
.PATH:	${.CURDIR}/${LSDIR}
SRCS+=	ls.c cmp.c print.c stat_flags.c util.c
CFLAGS+=-DINTERNAL_LS -Dmain=ls_main -I${.CURDIR}/${LSDIR}
.endif

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_EBONES)
.PATH:  ${.CURDIR}/../../usr.bin/login
SRCS+=	klogin.c
LDADD+=	-lkrb -ldes
DPADD+= ${LIBKRB} ${LIBDES}
CFLAGS+=-DKERBEROS
DISTRIBUTION=	krb
.endif

.include <bsd.prog.mk>
