#	$Id: Makefile,v 1.1.1.1 1997/09/24 20:37:14 markm Exp $

PROG=		telnet

CFLAGS+=	-DTERMCAP -DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DSKEY \
		-DENCRYPTION -DAUTHENTICATION -DKRB4 \
		-I${TELNETDIR}

SRCS=		authenc.c commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c tn3270.c utilities.c

DPADD=		${LIBTERMCAP} ${LIBTELNET} ${LIBDES} ${LIBKRB} ${LIBCRYPT}
LDADD=		-ltermcap -L${TELNETOBJDIR} -ltelnet -ldes \
		-L${KRBOBJDIR} -lkrb -lcrypt

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnet
