.PATH:		${.CURDIR}/..

PROG=	sshd
BINOWN=	root
BINMODE=555
BINDIR=	/usr/sbin
MAN=	sshd.8
CFLAGS+=-DHAVE_LOGIN_CAP

SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	pty.c log-server.c login.c servconf.c serverloop.c \
	auth.c auth1.c auth2.c auth-options.c session.c dh.c

.include <bsd.own.mk> # for KERBEROS and AFS

.if (${KERBEROS:L} == "yes")
.if (${AFS:L} == "yes")
CFLAGS+= -DAFS
LDADD+=  -lkafs
DPADD+=  ${LIBKRBAFS}
.endif # AFS
CFLAGS+= -DKRB4 -I${DESTDIR}/usr/include/kerberosIV
SRCS+=	auth-krb4.c
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}
.endif # KERBEROS

.if (${SKEY:L} == "yes")
SRCS+=	auth-skey.c auth2-skey.c
.endif

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+= -DLIBWRAP
LDADD+= -lwrap
DPADD+= ${LIBWRAP}
.endif

.if (${SKEY:L} == "yes")
CFLAGS+= -DSKEY
LDADD+= -lskey
DPADD+= ${SKEY}
.endif
