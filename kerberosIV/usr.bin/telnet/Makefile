# $FreeBSD$

PROG=		telnet

CFLAGS+=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DSKEY \
		-DENCRYPTION -DAUTHENTICATION -DKRB4 \
		-I${TELNETDIR} -DIPSEC -DINET6

SRCS=		authenc.c commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c tn3270.c utilities.c

DPADD=		${LIBTERMCAP} ${LIBTELNET} ${LIBCRYPTO} ${LIBKRB} ${LIBCRYPT} \
		${LIBCOM_ERR} ${LIBIPSEC}
LDADD=		-ltermcap -L${TELNETOBJDIR} -ltelnet -lcrypto \
		-L${KRBOBJDIR} -lkrb -lcrypt -lcom_err -lmp -lipsec

.if !defined(NOPAM)
CFLAGS+=-DUSE_PAM
DPADD+= ${LIBPAM}
LDADD+= ${MINUSLPAM}
.endif

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnet
