# $FreeBSD$
#

.include <bsd.own.mk>

PROG=	sshd
SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	sshpty.c sshlogin.c servconf.c serverloop.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-skey.c auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c \
	monitor_mm.c monitor.c monitor_wrap.c kexdhs.c kexgexs.c \
	auth-krb5.c \
	auth2-gss.c gss-serv.c gss-serv-krb5.c \
	loginrec.c auth-pam.c auth-shadow.c auth-sia.c md5crypt.c \
	audit.c audit-bsm.c platform.c sftp-server.c sftp-common.c \
	gss-genr.c

# gss-genr.c really belongs in libssh; see src/secure/lib/libssh/Makefile

MAN=	sshd.8 sshd_config.5
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBUTIL} ${LIBZ} ${LIBWRAP} ${LIBPAM}
LDADD=	-lssh -lutil -lz -lwrap ${MINUSLPAM}

.if ${MK_AUDIT} != "no"
CFLAGS+= -DUSE_BSM_AUDIT -DHAVE_GETAUDIT_ADDR
DPADD+=  ${LIBBSM}
LDADD+=  -lbsm
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -DGSSAPI -DHAVE_GSSAPI_GSSAPI_H=1 -DHAVE_GSSAPI_GSSAPI_KRB5_H=1 -DKRB5 -DHEIMDAL
DPADD+=	 ${LIBGSSAPI} ${LIBGSSAPI_KRB5}
LDADD+=	 -lgssapi -lgssapi_krb5
.endif

.if defined(X11BASE)
# Recommended /etc/make.conf setting is X11BASE=${LOCALBASE} for x.org
# 7.x upgrade on <= 6.2, but LOCALBASE has moved out of scope of src/
# so we need to provide the default for users with old make.conf
# settings.
LOCALBASE?=	/usr/local

# Users may override either LOCALBASE or X11BASE to move the location
# of xauth
X11BASE?=	${LOCALBASE}
CFLAGS+= -DXAUTH_PATH=\"${X11BASE}/bin/xauth\"
.endif

DPADD+=	${LIBCRYPTO} ${LIBCRYPT}
LDADD+=	-lcrypto -lcrypt

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
