#	$NetBSD: Makefile,v 1.56 2006/03/23 20:14:16 wiz Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

.include <bsd.own.mk>

PROG=	ftpd
SRCS=	cmds.c conf.c ftpd.c ftpcmd.y logutmp.c logwtmp.c popen.c
CPPFLAGS+=-I${.CURDIR} -DSUPPORT_UTMP -DSUPPORT_UTMPX -DLOGIN_CAP
DPADD+=	${LIBCRYPT} ${LIBUTIL}
LDADD+=	-lcrypt -lutil
MAN=	ftpd.conf.5 ftpusers.5 ftpd.8
MLINKS=	ftpusers.5 ftpchroot.5

# for `internal' ls
SRCS+=	ls.c cmp.c print.c stat_flags.c util.c
.PATH:	${NETBSDSRCDIR}/bin/ls

.if (${USE_INET6} != "no")
CPPFLAGS+=-DINET6
.endif

WARNS=2

.if (${USE_SKEY} != "no")
CPPFLAGS+=-DSKEY
DPADD+= ${LIBSKEY}
LDADD+= -lskey
.endif
.if (${USE_PAM} != "no")
CPPFLAGS+=-DUSE_PAM
DPADD+= ${LIBPAM} ${PAM_STATIC_DPADD}
LDADD+= -lpam ${PAM_STATIC_LDADD}
.endif

ftpd.o ftpcmd.o: version.h

#.if (${USE_KERBEROS} != "no")
#
#.PATH:	${NETBSDSRCDIR}/usr.bin/login
#
#SRCS+=	k5login.c
#CPPFLAGS+=-DKERBEROS5
#CPPFLAGS+=-DKERBEROS5 -I${DESTDIR}/usr/include/krb5
#DPADD+=	${LIBKRB5} ${LIBASN1}
#LDADD+= -lkrb5 -lasn1
#
#DPADD+=	${LIBCRYPTO} ${{LIBCRYPT} ${LIBROKEN} ${LIBCOM_ERR}
#LDADD+=	-lcrypto -lcrypt -lroken -lcom_err
#
#.endif

.include <bsd.prog.mk>
