#	$Id$

PROG=		telnet

CFLAGS+=	-DTERMCAP -DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DSKEY \
		-DENCRYPTION -DAUTHENTICATION -DKRB4 \
		-I${TELNETDIR}

SRCS=		authenc.c commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c tn3270.c utilities.c

DPADD=		${LIBTERMCAP} ${LIBTELNET} ${LIBDES} ${LIBKRB}
LDADD=		-ltermcap -L${TELNETOBJDIR} -ltelnet -ldes -L${KRBOBJDIR} -lkrb

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnet
