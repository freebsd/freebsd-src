# $FreeBSD$

PROG=		telnet

CFLAGS=		-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DSKEY \
		-DENCRYPTION -DAUTHENTICATION -DIPSEC -DINET6 \
		-I${TELNETDIR}
#CFLAGS+=	-DKRB5

SRCS=		authenc.c commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c tn3270.c utilities.c

DPADD=		${LIBTERMCAP} ${LIBTELNET} ${LIBMP} ${LIBCRYPTO} ${LIBCRYPT} \
		${LIBIPSEC} ${LIBPAM}
LDADD=		-ltermcap ${LIBTELNET} -lmp -lcrypto -lcrypt \
		-lipsec ${MINUSLPAM}

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnet
