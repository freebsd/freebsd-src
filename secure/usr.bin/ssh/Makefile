# $FreeBSD$

.include <bsd.own.mk>

PROG=	ssh
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MAN=	ssh.1 ssh_config.5
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c mux.c

# gss-genr.c really belongs in libssh; see src/secure/lib/libssh/Makefile
SRCS+=	gss-genr.c

CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
SRCS+=	ssh_namespace.h

DPADD=	${LIBSSH} ${LIBUTIL}
LDADD=	-lssh -lutil
USEPRIVATELIB= ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
DPADD+=	${LIBLDNS}
LDADD+=	-lldns
USEPRIVATELIB+= ldns
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -include krb5_config.h
SRCS+=	krb5_config.h
DPADD+=	 ${LIBGSSAPI}
LDADD+=	 -lgssapi
.endif

DPADD+= ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD+= -lcrypt -lcrypto -lz

.if defined(LOCALBASE)
CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE}/bin/xauth\"
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
