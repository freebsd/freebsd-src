#	$OpenBSD: Makefile,v 1.51 2002/06/20 19:56:07 stevesk Exp $

.PATH:		${.CURDIR}/..

PROG=	sshd
BINOWN=	root
BINMODE=555
BINDIR=	/usr/sbin
MAN=	sshd.8 sshd_config.5
CFLAGS+=-DHAVE_LOGIN_CAP -DBSD_AUTH

SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	sshpty.c sshlogin.c servconf.c serverloop.c uidswap.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-skey.c auth-bsdauth.c monitor_mm.c monitor.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c \
	auth2-hostbased.c auth2-kbdint.c

.include <bsd.own.mk> # for KERBEROS and AFS

.if (${KERBEROS5:L} == "yes")
CFLAGS+=-DKRB5 -I${DESTDIR}/usr/include/kerberosV
SRCS+=  auth-krb5.c
LDADD+= -lkrb5 -lkafs -lasn1 -lcom_err
DPADD+= ${LIBKRB5} ${LIBKAFS} ${LIBASN1} ${LIBCOM_ERR}
.endif # KERBEROS5

.if (${KERBEROS:L} == "yes")
.if (${AFS:L} == "yes")
CFLAGS+= -DAFS
LDADD+=  -lkafs
DPADD+=  ${LIBKAFS}
.endif # AFS
CFLAGS+= -DKRB4 -I${DESTDIR}/usr/include/kerberosIV
SRCS+=	auth-krb4.c
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}
.endif # KERBEROS

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz -ldes
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ} ${LIBDES}

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+= -DLIBWRAP
LDADD+= -lwrap
DPADD+= ${LIBWRAP}
.endif

#.if (${SKEY:L} == "yes")
#CFLAGS+= -DSKEY
#LDADD+= -lskey
#DPADD+= ${SKEY}
#.endif
