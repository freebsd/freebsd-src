# $FreeBSD$

TELNETDIR=	${.CURDIR}/../../contrib/telnet
.PATH:		${TELNETDIR}/telnet

PROG=		telnet

SRCS=		commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c utilities.c

CFLAGS+=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DOPIE \
		-I${TELNETDIR} -I${TELNETDIR}/libtelnet/

.if exists(${.OBJDIR}/../../lib/libtelnet)
LIBTELNET=	${.OBJDIR}/../../lib/libtelnet/libtelnet.a
.else
LIBTELNET=	${.CURDIR}/../../lib/libtelnet/libtelnet.a
.endif

DPADD=		${LIBTERMCAP} ${LIBTELNET}
LDADD=		-ltermcap ${LIBTELNET}

.if !defined(RELEASE_CRUNCH)
CFLAGS+=	-DINET6 -DIPSEC
DPADD+=		${LIBIPSEC}
LDADD+=		-lipsec
.else
.PATH:         ${TELNETDIR}/libtelnet
SRCS+=		genget.c getent.c misc.c
CFLAGS+=	-DHAS_CGETENT
.endif

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
SRCS+=		authenc.c 
CFLAGS+=	-DENCRYPTION -DAUTHENTICATION -DIPSEC
DPADD+=		${LIBMP} ${LIBCRYPTO} ${LIBCRYPT} ${LIBIPSEC} ${LIBPAM}
LDADD+=		-lmp -lcrypto -lcrypt -lipsec ${MINUSLPAM}
.if !defined(NO_KERBEROS)
CFLAGS+=	-DKRB5 -DFORWARD -Dnet_write=telnet_net_write
DPADD+=		${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR} ${LIBROKEN}
LDADD+=		-lkrb5 -lasn1 -lcom_err -lroken
.endif
.endif

.include <bsd.prog.mk>
