#	$NetBSD: Makefile,v 1.49 2002/08/22 00:09:38 christos Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

.include <bsd.own.mk>

PROG=	ftpd
SRCS=	cmds.c conf.c ftpd.c ftpcmd.y logutmp.c logwtmp.c popen.c
CPPFLAGS+=-I${.CURDIR} -DSUPPORT_UTMP -DSUPPORT_UTMPX
DPADD+=	${LIBCRYPT} ${LIBUTIL}
LDADD+=	-lcrypt -lutil
MAN=	ftpd.conf.5 ftpusers.5 ftpd.8
MLINKS=	ftpusers.5 ftpchroot.5

# for `internal' ls
SRCS+=	ls.c cmp.c print.c stat_flags.c util.c
.PATH:	${NETBSDSRCDIR}/bin/ls

CPPFLAGS+=-DINET6
.include <bsd.own.mk>

WARNS=2

.if (${USE_SKEY} != "no")
CPPFLAGS+=-DSKEY
DPADD+= ${LIBSKEY}
LDADD+= -lskey
.endif

ftpd.o ftpcmd.o: version.h

# XXX Kerberos support is broken right now.
#.PATH:	${NETBSDSRCDIR}/usr.bin/login

#.ifdef KERBEROS5
#SRCS+=	k5login.c
#CPPFLAGS+=-DKERBEROS5
#DPADD+= ${LIBKRB5} ${LIBK5CRYPTO} ${LIBCOM_ERR}
#LDADD+= -lkrb5 -lk5crypto -lcom_err
#.else
#SRCS+=	klogin.c
#CPPFLAGS+=-DKERBEROS
#DPADD+= ${LIBKRB} ${LIBDES} ${LIBCOM_ERR}
#LDADD+= -lkrb -kdes -lcom_err
#.endif

.include <bsd.prog.mk>
