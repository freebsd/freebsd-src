# $FreeBSD$
#

PROG=	sshd
SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	sshpty.c sshlogin.c servconf.c serverloop.c uidswap.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-skey.c auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c \
	monitor_mm.c monitor.c monitor_wrap.c monitor_fdpass.c \
	kexdhs.c kexgexs.c \
	auth-krb5.c \
	auth2-gss.c gss-serv.c gss-serv-krb5.c \
	loginrec.c auth-pam.c auth-sia.c md5crypt.c
MAN=	sshd.8 sshd_config.5
CFLAGS+=-I${SSHDIR}

.if !defined(NO_KERBEROS)
CFLAGS+= -DGSSAPI -DKRB5 -DHEIMDAL
DPADD+=	 ${LIBGSSAPI} ${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR} ${LIBMD} ${LIBROKEN}
LDADD+=	 -lgssapi -lkrb5 -lasn1 -lcom_err -lmd -lroken
.endif

DPADD+=	${LIBOPIE} ${LIBMD}
LDADD+=	-lopie -lmd

.if defined(X11BASE)
CFLAGS+= -DXAUTH_PATH=\"${X11BASE}/bin/xauth\"
.endif

DPADD+=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ} ${LIBWRAP} ${LIBPAM}
LDADD+=	-lssh -lcrypt -lcrypto -lutil -lz -lwrap ${MINUSLPAM}

.if defined(OPENSSH_USE_POSIX_THREADS) && !defined(NOLIBC_R)
CFLAGS+=-DUSE_POSIX_THREADS
DPADD+= ${LIBC_R}
LDADD+= -lc_r
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
