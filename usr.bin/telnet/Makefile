
#	@(#)Makefile	5.6 (Berkeley) 3/5/91
#

PROG=	telnet

CFLAGS+=-DTERMCAP -DKLUDGELINEMODE -DUSE_TERMIO
CFLAGS+=-I${.CURDIR}/../../lib

.if exists(/usr/lib/libcrypt.a)
#CFLAGS+= -DENCRYPT
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

LDADD+=	-ltermcap -ltelnet
DPADD+=	${LIBTERMCAP} ${LIBTELNET}


SRCS=	authenc.c commands.c main.c network.c ring.c sys_bsd.c telnet.c \
	terminal.c tn3270.c utilities.c

.include <bsd.prog.mk>
