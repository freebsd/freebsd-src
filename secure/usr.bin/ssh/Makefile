# $FreeBSD$
#

PROG=	ssh
CFLAGS+=-I${SSHDIR}
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MAN=	ssh.1 ssh_config.5
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c

.if defined(MAKE_KERBEROS4)
DISTRIBUTION=krb4
CFLAGS+= -DKRB4
LDADD+=	 -lkrb -lcom_err
DPADD+=	 ${LIBKRB} ${LIBCOM_ERR}
.endif

.if defined(MAKE_KERBEROS5)
DISTRIBUTION=krb5
CFLAGS+= -DKRB5 -DHEIMDAL
LDADD+=	 -lkrb5 -lasn1 -lcom_err -lmd -L${.OBJDIR}/../../../kerberos5/lib/libroken -lroken -lcrypt
DPADD+=	 ${LIBKRB5} ${LIBCOM_ERR} ${LIBASN1} ${LIBMD} ${LIBCRYPT}
.endif

.if defined(X11BASE)
CFLAGS+= -DXAUTH_PATH=\"${X11BASE}/bin/xauth\"
.endif

LDADD+=	-lssh -lcrypto -lutil -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
