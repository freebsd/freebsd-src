#	$Id: Makefile,v 1.9.2.4 1997/05/11 12:58:57 brian Exp $

PROG=	ppp
SRCS=	alias_cmd.c arp.c async.c auth.c ccp.c chap.c chat.c command.c \
	filter.c fsm.c hdlc.c ip.c ipcp.c lcp.c log.c lqr.c main.c mbuf.c \
	modem.c os.c pap.c passwdauth.c pred.c route.c sig.c slcompress.c \
	systems.c timer.c vars.c vjcomp.c
#CFLAGS+= -DHAVE_SHELL_CMD_WITH_ANY_MODE
CFLAGS += -Wall -DMSEXT -DPASSWDAUTH
LDADD += -lmd -lcrypt -lutil -lalias
DPADD += ${LIBMD} ${LIBCRYPT} ${LIBUTIL} ${LIBALIAS}
MAN8=	ppp.8
BINMODE=4555
BINOWN=	root

.include <bsd.prog.mk>
