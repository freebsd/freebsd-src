.PATH:		${.CURDIR}/..

PROG=	ssh
BINOWN=	root

BINMODE?=4555

BINDIR=	/usr/bin
MAN=	ssh.1
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c log-client.c readconf.c clientloop.c \
	sshconnect.c sshconnect1.c sshconnect2.c

.include <bsd.own.mk> # for AFS

.if (${KERBEROS:L} == "yes")
CFLAGS+= -DKRB4 -I${DESTDIR}/usr/include/kerberosIV
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}
.if (${AFS:L} == "yes")
CFLAGS+= -DAFS
LDADD+=  -lkafs
DPADD+=  ${LIBKRBAFS}
.endif # AFS
.endif # KERBEROS

.include <bsd.prog.mk>

LDADD+=	-lutil -lz -lcrypto
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
