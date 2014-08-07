# $FreeBSD$

.include <src.opts.mk>

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

DPADD=	${LIBSSH} ${LIBUTIL}
LDADD=	${LDSSH} -lutil
USEPRIVATELIB= ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
DPADD+=	${LIBLDNS}
LDADD+=	${LIBLDNSADD}
USEPRIVATELIB+= ldns
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -include krb5_config.h
DPADD+=	 ${LIBGSSAPI}
LDADD+=	 -lgssapi
.endif

.if ${MK_OPENSSH_NONE_CIPHER} != "no"
CFLAGS+= -DNONE_CIPHER_ENABLED
.endif

DPADD+= ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD+= -lcrypt -lcrypto -lz

.if defined(LOCALBASE)
CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE}/bin/xauth\"
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
.if ${MK_KERBEROS_SUPPORT} != "no"
${OBJS} ${POBJS} ${SOBJS}: krb5_config.h
.endif
