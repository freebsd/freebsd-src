#	$NetBSD: Makefile,v 1.51 2003-07-23 08:01:27 itojun Exp $
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
#SRCS+= klogin.c
#CPPFLAGS+=-DKERBEROS -I${DESTDIR}/usr/include/kerberosIV
#DPADD+=	${LIBKRB}
#LDADD+= -lkrb
#
#DPADD+=	${LIBCRYPTO} ${LIBROKEN} ${LIBCOM_ERR}
#LDADD+=	-lcrypto -lroken -lcom_err
#
#.endif

.include <bsd.prog.mk>
