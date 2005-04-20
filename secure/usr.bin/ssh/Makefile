# $FreeBSD$
#

PROG=	ssh
CFLAGS+=-I${SSHDIR}
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MAN=	ssh.1 ssh_config.5
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c \
	gss-genr.c

DPADD=	${LIBSSH} ${LIBUTIL} ${LIBZ}
LDADD=	-lssh -lutil -lz

.if !defined(NO_KERBEROS)
CFLAGS+= -DGSSAPI -DKRB5 -DHEIMDAL
DPADD+=	 ${LIBGSSAPI} ${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR} ${LIBMD} ${LIBROKEN}
LDADD+=	 -lgssapi -lkrb5 -lasn1 -lcom_err -lmd -lroken
.endif

.if defined(X11BASE)
CFLAGS+= -DXAUTH_PATH=\"${X11BASE}/bin/xauth\"
.endif

DPADD+=	${LIBCRYPT} ${LIBCRYPTO}
LDADD+=	-lcrypt -lcrypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
