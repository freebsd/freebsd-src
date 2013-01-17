# $FreeBSD$
#

.include <bsd.own.mk>

PROG=	ssh
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MAN=	ssh.1 ssh_config.5
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c mux.c \
	roaming_common.c roaming_client.c

# gss-genr.c really belongs in libssh; see src/secure/lib/libssh/Makefile
SRCS+=	gss-genr.c

DPADD=	${LIBSSH} ${LIBUTIL} ${LIBZ}
LDADD=	-lssh -lutil -lz

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -DGSSAPI -DHAVE_GSSAPI_GSSAPI_H=1 -DKRB5 -DHEIMDAL
DPADD+=	 ${LIBGSSAPI}
LDADD+=	 -lgssapi
.endif

.if ${MK_OPENSSH_NONE_CIPHER} != "no"
CFLAGS+= -DNONE_CIPHER_ENABLED
.endif

DPADD+=	${LIBCRYPT} ${LIBCRYPTO}
LDADD+=	-lcrypt -lcrypto

.if defined(LOCALBASE)
CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE}/bin/xauth\"
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
