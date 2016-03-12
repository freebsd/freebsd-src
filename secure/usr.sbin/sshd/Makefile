# $FreeBSD$

.include <bsd.own.mk>

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

# gss-genr.c really belongs in libssh; see src/secure/lib/libssh/Makefile
SRCS+=	gss-genr.c

MAN=	sshd.8 sshd_config.5
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
SRCS+=	ssh_namespace.h

DPADD=	${LIBSSH} ${LIBUTIL} ${LIBWRAP} ${LIBPAM}
LDADD=	-lssh -lutil -lwrap ${MINUSLPAM}
USEPRIVATELIB= ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
#DPADD+=	${LIBLDNS}
#LDADD+=	-lldns
#USEPRIVATELIB+= ldns
.endif

.if ${MK_AUDIT} != "no"
CFLAGS+= -DUSE_BSM_AUDIT -DHAVE_GETAUDIT_ADDR
DPADD+=  ${LIBBSM}
LDADD+=  -lbsm
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -include krb5_config.h
SRCS+=	krb5_config.h
DPADD+=	 ${LIBGSSAPI_KRB5} ${LIBGSSAPI} ${LIBKRB5} ${LIBHX509} ${LIBASN1} \
	${LIBCOM_ERR} ${LIBROKEN} ${LIBWIND} ${LIBHEIMBASE} ${LIBHEIMIPCC}
LDADD+=	 -lgssapi_krb5 -lgssapi -lkrb5 -lhx509 -lasn1 \
	-lcom_err -lroken -lwind -lheimbase -lheimipcc
.endif

DPADD+= ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD+= -lcrypt -lcrypto -lz

# Fix the order of NEEDED entries for libthr and libc. The libthr
# needs to interpose libc symbols, leaving the libthr loading as
# dependency of krb causes reversed order and broken interposing. Put
# the threading library last on the linker command line, just before
# the -lc added by a compiler driver.
.if ${MK_KERBEROS_SUPPORT} != "no"
DPADD+= ${LIBPTHREAD}
LDADD+= -lpthread
.endif

.if defined(LOCALBASE)
CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE}/bin/xauth\"
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
