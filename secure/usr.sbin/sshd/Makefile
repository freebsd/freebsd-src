# $FreeBSD$
#

LOGINSRC= ${.CURDIR}/../../../usr.bin/login

PROG=	sshd
SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	sshpty.c sshlogin.c servconf.c serverloop.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c auth-skey.c auth-pam.c login_access.c \
	groupaccess.c
MAN=	sshd.8

CFLAGS+= -DLIBWRAP -DHAVE_LOGIN_CAP -DLOGIN_ACCESS -I${LOGINSRC} -DUSE_PAM -DHAVE_PAM_GETENVLIST

.if defined(MAKE_KERBEROS4) && \
	((${MAKE_KERBEROS4} == "yes") || (${MAKE_KERBEROS4} == "YES"))
DISTRIBUTION=krb4
CFLAGS+= -DKRB4
SRCS+=	auth-krb4.c
LDADD+=	 -lkrb -lcom_err
DPADD+=	 ${LIBKRB} ${LIBCOM_ERR}
.endif # MAKE_KERBEROS4

.if defined(MAKE_KERBEROS5) && \
	((${MAKE_KERBEROS5} == "yes") || (${MAKE_KERBEROS5} == "YES"))
DISTRIBUTION=krb5
CFLAGS+= -DKRB5
SRCS+=	auth-krb5.c
LDADD+=	 -lkrb5 -lasn1 -lcom_err -lmd -L${.OBJDIR}/../../../kerberos5/lib/libroken -lroken
DPADD+=	 ${LIBKRB5} ${LIBCOM_ERR} ${LIBASN1} ${LIBMD}
.endif # MAKE_KERBEROS5

CFLAGS+= -DSKEY
LDADD+=	-lopie -lmd
DPADD+=	${LIBOPIE} ${LIBMD}

.if defined(X11BASE)
CFLAGS+= -DXAUTH_PATH=\"${X11BASE}/bin/xauth\"
.endif

LDADD+=	-lssh -lcrypt -lcrypto -lutil -lz -lwrap ${MINUSLPAM}
DPADD+=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ} ${LIBWRAP} ${LIBPAM}

.include <bsd.prog.mk>

.PATH:	${SSHDIR} ${LOGINSRC}
