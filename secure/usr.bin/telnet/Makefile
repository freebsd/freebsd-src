#       $FreeBSD$

PROG=		telnet

CFLAGS+=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DSKEY \
                -DENCRYPTION -DAUTHENTICATION -I${TELNETDIR}

SRCS=		authenc.c commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c tn3270.c utilities.c

DPADD=		${LIBTERMCAP} ${LIBTELNET} ${LIBDES} ${LIBCRYPT} ${LIBMP}
LDADD=		-ltermcap -L${TELNETOBJDIR} -ltelnet -ldes -lcrypt -lmp

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnet
