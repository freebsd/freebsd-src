# $FreeBSD$

PROG=		telnet

SRCS=		authenc.c commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c utilities.c

CFLAGS+=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DOPIE \
		-DENCRYPTION -DAUTHENTICATION -DIPSEC -DINET6 \
		-I${TELNETDIR} -I${TELNETDIR}/libtelnet/

CFLAGS+=	-DKRB5 -DFORWARD -Dnet_write=telnet_net_write

WARNS?=		2

DPADD=
LDADD=

.if defined(MAKE_KERBEROS4)
DPADD+=		${LIBKRB} ${LIBCOM_ERR}
LDADD+=		-lkrb -lcom_err
.endif

DPADD+=		${LIBTERMCAP} ${LIBTELNET} ${LIBMP} \
		${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR} ${LIBROKEN} \
		${LIBCRYPTO} ${LIBCRYPT} ${LIBIPSEC} ${LIBPAM}
LDADD+=		-ltermcap ${LIBTELNET} -lmp \
		-L${KRB5OBJDIR} -lkrb5 -L${ASN1OBJDIR} -lasn1 -lcom_err \
		-L${ROKENOBJDIR} -lroken \
		-lcrypto -lcrypt -lipsec ${MINUSLPAM}

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnet
