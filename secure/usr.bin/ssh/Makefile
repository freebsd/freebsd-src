# $FreeBSD$

.include <src.opts.mk>

CONFS=	ssh_config
CONFSDIR=	/etc/ssh
PROG=	ssh
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MAN=	ssh.1 ssh_config.5
MLINKS=	ssh.1 slogin.1
PACKAGE=	ssh

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect2.c mux.c

# gss-genr.c really belongs in libssh; see src/secure/lib/libssh/Makefile
SRCS+=	gss-genr.c

CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
SRCS+=	ssh_namespace.h

LIBADD=	ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
.endif

.if ${MK_GSSAPI} != "no" && ${MK_KERBEROS_SUPPORT} != "no"
KRB5DIR=	${SRCTOP}/crypto/heimdal
KERBEROS5_DIR=	${SRCTOP}/kerberos5
CFLAGS+= -include krb5_config.h \
	-I${KERBEROS5_DIR}/include \
	-I${KRB5DIR}/lib/krb5
SRCS+=	krb5_config.h
LIBADD+=	gssapi
.endif

LIBADD+=	crypto

.if defined(LOCALBASE)
CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE}/bin/xauth\"
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
