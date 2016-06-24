# $FreeBSD$

.include <src.opts.mk>

PROG=	sshd
SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	audit.c audit-bsm.c audit-linux.c platform.c \
	sshpty.c sshlogin.c servconf.c serverloop.c \
	auth.c auth1.c auth2.c auth-options.c session.c \
	auth-chall.c auth2-chall.c groupaccess.c \
	auth-skey.c auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c \
	monitor_mm.c monitor.c monitor_wrap.c auth-krb5.c \
	auth2-gss.c gss-serv.c gss-serv-krb5.c \
	loginrec.c auth-pam.c auth-shadow.c auth-sia.c md5crypt.c \
	sftp-server.c sftp-common.c \
	sandbox-null.c sandbox-rlimit.c sandbox-systrace.c sandbox-darwin.c \
	sandbox-seccomp-filter.c sandbox-capsicum.c sandbox-pledge.c \
	sandbox-solaris.c
PACKAGE=	ssh

# gss-genr.c really belongs in libssh; see src/secure/lib/libssh/Makefile
SRCS+=	gss-genr.c

MAN=	sshd.8 sshd_config.5
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
SRCS+=	ssh_namespace.h

# pam should always happen before ssh here for static linking
LIBADD=	pam ssh util wrap

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
#DPADD+=	${LIBLDNS}
#LDADD+=	-lldns
.endif

.if ${MK_AUDIT} != "no"
CFLAGS+= -DUSE_BSM_AUDIT -DHAVE_GETAUDIT_ADDR
LIBADD+=	bsm
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -include krb5_config.h
SRCS+=	krb5_config.h
LIBADD+=	gssapi_krb5 gssapi krb5
.endif

LIBADD+=	crypto

.if defined(LOCALBASE)
CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE}/bin/xauth\"
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
