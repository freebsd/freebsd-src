#	$Id: Makefile,v 1.9.2.5 1997/05/24 10:29:22 brian Exp $

PROG=	ppp
SRCS=	alias_cmd.c arp.c async.c auth.c ccp.c chap.c chat.c command.c \
	filter.c fsm.c hdlc.c ip.c ipcp.c lcp.c loadalias.c log.c lqr.c \
	main.c mbuf.c modem.c os.c pap.c passwdauth.c pred.c route.c sig.c \
	slcompress.c systems.c timer.c vars.c vjcomp.c
#CFLAGS+= -DHAVE_SHELL_CMD_WITH_ANY_MODE
CFLAGS += -Wall -DMSEXT -DPASSWDAUTH
LDADD += -lmd -lcrypt -lutil
DPADD += ${LIBMD} ${LIBCRYPT} ${LIBUTIL}
MAN8=	ppp.8
BINMODE=4555
BINOWN=	root

.include <bsd.prog.mk>
