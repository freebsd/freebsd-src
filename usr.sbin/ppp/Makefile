#	$Id: Makefile,v 1.5 1996/01/30 20:04:20 phk Exp $

PROG=	ppp
SRCS=	async.c auth.c ccp.c chap.c chat.c command.c filter.c fsm.c hdlc.c \
	ip.c ipcp.c lcp.c lqr.c log.c main.c mbuf.c modem.c os.c \
	pap.c pred.c route.c slcompress.c timer.c systems.c uucplock.c vars.c \
	vjcomp.c arp.c
#CFLAGS+= -DHAVE_SHELL_CMD_WITH_ANY_MODE
CFLAGS += -Wall -DUSE_PERROR
LDADD += -lmd
DPADD += ${LIBMD}
MAN8=	ppp.8
BINMODE=4555
BINOWN=	root

.PATH: ${.CURDIR}/../../sbin/startslip

.include <bsd.prog.mk>
