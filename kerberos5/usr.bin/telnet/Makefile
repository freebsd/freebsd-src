# $FreeBSD$

PROG=		telnet

CFLAGS+=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DOPIE \
		-DENCRYPTION -DAUTHENTICATION -DIPSEC -DINET6 \
		-I${TELNETDIR}

.if defined(MAKE_KERBEROS4)
CFLAGS+=	-DKRB4
.endif

#CFLAGS+=	-DKRB5

SRCS=		authenc.c commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c tn3270.c utilities.c

DPADD=
LDADD=

.if defined(MAKE_KERBEROS4)
DPADD+=		${LIBKRB} ${LIBCOM_ERR}
LDADD+=		-lkrb -lcom_err
.endif

DPADD+=		${LIBTERMCAP} ${LIBTELNET} ${LIBMP} ${LIBCRYPTO} ${LIBCRYPT} \
		${LIBIPSEC} ${LIBPAM}
LDADD+=		-ltermcap ${LIBTELNET} -lmp -lcrypto -lcrypt \
		-lipsec ${MINUSLPAM}

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnet
