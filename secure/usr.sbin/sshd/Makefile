# $FreeBSD$
#

PROG=	sshd
SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	sshpty.c sshlogin.c servconf.c serverloop.c uidswap.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-skey.c auth-bsdauth.c monitor_mm.c monitor.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c \
	auth2-hostbased.c auth2-kbdint.c \
	auth2-pam-freebsd.c monitor_wrap.c monitor_fdpass.c \
	kexdhs.c kexgexs.c
# Portability layer
SRCS+=	loginrec.c
MAN=	sshd.8 sshd_config.5
CFLAGS+=-I${SSHDIR}

.if !defined(NO_KERBEROS)
CFLAGS+= -DKRB5 -DHEIMDAL
SRCS+=	auth-krb5.c
LDADD+=	 -lkrb5 -lasn1 -lcom_err -lmd -lroken
DPADD+=	 ${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR} ${LIBMD} ${LIBROKEN}
.endif

LDADD+=	-lopie -lmd
DPADD+=	${LIBOPIE} ${LIBMD}

.if defined(X11BASE)
CFLAGS+= -DXAUTH_PATH=\"${X11BASE}/bin/xauth\"
.endif

LDADD+=	-lssh -lcrypt -lcrypto -lutil -lz -lwrap ${MINUSLPAM}
DPADD+=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ} ${LIBWRAP} ${LIBPAM}

.if defined(OPENSSH_USE_POSIX_THREADS) && !defined(NOLIBC_R)
CFLAGS+=-DUSE_POSIX_THREADS
LDADD+= -lc_r
DPADD+= ${LIBC_R}
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
