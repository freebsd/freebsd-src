#	@(#)Makefile	8.2 (Berkeley) 4/4/94
#	$Id: Makefile,v 1.24 1998/05/04 18:20:18 bde Exp $

PROG=	ftpd
MAN8=	ftpd.8
SRCS=	ftpd.c ftpcmd.y logwtmp.c popen.c skey-stuff.c

CFLAGS+=-DSETPROCTITLE -DSKEY -DLOGIN_CAP -DVIRTUAL_HOSTING -Wall \
	-I${.CURDIR}/../../contrib-crypto/telnet
YFLAGS=

LDADD=	-lskey -lmd -lcrypt -lutil
DPADD=	${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}

.ifdef FTPD_INTERNAL_LS
LSDIR=	../../bin/ls
.PATH:	${.CURDIR}/${LSDIR}
SRCS+=	ls.c cmp.c print.c stat_flags.c util.c
CFLAGS+=-DINTERNAL_LS -Dmain=ls_main -I${.CURDIR}/${LSDIR}
.endif

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_KERBEROS4)
.PATH:  ${.CURDIR}/../../lib/libpam/modules/pam_kerberosIV
SRCS+=	klogin.c
LDADD+=	-lkrb -ldes
DPADD+= ${LIBKRB} ${LIBDES}
CFLAGS+=-DKERBEROS
DISTRIBUTION=	krb
.endif

.include <bsd.prog.mk>
