# $FreeBSD$

PROG=		telnet

SRCS=		authenc.c commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c utilities.c

CFLAGS+=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DOPIE \
		-DENCRYPTION -DAUTHENTICATION -DIPSEC -DINET6 \
		-I${TELNETDIR} -I${TELNETDIR}/libtelnet/

WARNS?=		2

DPADD=		${LIBTERMCAP} ${LIBTELNET} ${LIBMP} \
		${LIBCRYPTO} ${LIBCRYPT} ${LIBIPSEC} ${LIBPAM}
LDADD=		-ltermcap ${LIBTELNET} -lmp \
		-lcrypto -lcrypt -lipsec ${MINUSLPAM}

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnet
