# $FreeBSD$
#

.include <bsd.own.mk>

PROG=	sshd
SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	audit.c audit-bsm.c audit-linux.c platform.c \
	sshpty.c sshlogin.c servconf.c serverloop.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-skey.c auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c auth2-jpake.c \
	monitor_mm.c monitor.c monitor_wrap.c kexdhs.c kexgexs.c kexecdhs.c \
	auth-krb5.c \
	auth2-gss.c gss-serv.c gss-serv-krb5.c \
	loginrec.c auth-pam.c auth-shadow.c auth-sia.c md5crypt.c \
	sftp-server.c sftp-common.c \
	roaming_common.c roaming_serv.c \
	sandbox-null.c sandbox-rlimit.c sandbox-systrace.c sandbox-darwin.c

# gss-genr.c really belongs in libssh; see src/secure/lib/libssh/Makefile
SRCS+=	gss-genr.c

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
DPADD+=	 ${LIBGSSAPI_KRB5} ${LIBGSSAPI} ${LIBKRB5} ${LIBASN1}
LDADD+=	 -lgssapi_krb5 -lgssapi -lkrb5 -lasn1
.endif

.if ${MK_OPENSSH_NONE_CIPHER} != "no"
CFLAGS+= -DNONE_CIPHER_ENABLED
.endif

DPADD+=	${LIBCRYPTO} ${LIBCRYPT}
LDADD+=	-lcrypto -lcrypt

.if defined(LOCALBASE)
CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE}/bin/xauth\"
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
