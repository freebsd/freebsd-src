#	$Id: Makefile,v 1.7 1996/10/06 13:32:26 jkh Exp $

PROG=	ppp
SRCS=	async.c auth.c ccp.c chap.c chat.c command.c filter.c fsm.c hdlc.c \
	ip.c ipcp.c lcp.c lqr.c log.c main.c mbuf.c modem.c os.c \
	pap.c pred.c route.c slcompress.c timer.c systems.c uucplock.c vars.c \
	vjcomp.c arp.c passwdauth.c
#CFLAGS+= -DHAVE_SHELL_CMD_WITH_ANY_MODE
CFLAGS += -Wall -DUSE_PERROR -DMSEXT -DPASSWDAUTH # -DLOCALHACK
LDADD += -lmd -lcrypt -lutil
DPADD += ${LIBMD}
MAN8=	ppp.8
BINMODE=4555
BINOWN=	root

.PATH: ${.CURDIR}/../../sbin/startslip

.include <bsd.prog.mk>
