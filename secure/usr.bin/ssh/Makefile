# $FreeBSD$
#

PROG=	ssh
CFLAGS+=-I${SSHDIR}
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MAN=	ssh.1 ssh_config.5
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c authfd.c hostfile.c msg.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c

.if !defined(NO_KERBEROS)
CFLAGS+= -DKRB5 -DHEIMDAL
LDADD+=	 -lkrb5 -lasn1 -lcom_err -lmd -lroken -lcrypt
DPADD+=	 ${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR} ${LIBMD} ${LIBROKEN} ${LIBCRYPT}
.endif

.if defined(X11BASE)
CFLAGS+= -DXAUTH_PATH=\"${X11BASE}/bin/xauth\"
.endif

LDADD+=	-lssh -lcrypto -lutil -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
