#	$Id: Makefile,v 1.16 1997/03/13 12:45:26 brian Exp $

PROG=	ppp
SRCS=	async.c auth.c ccp.c chap.c chat.c command.c filter.c fsm.c hdlc.c \
	ip.c ipcp.c lcp.c lqr.c log.c main.c mbuf.c modem.c os.c \
	pap.c pred.c route.c slcompress.c timer.c systems.c vars.c \
	vjcomp.c arp.c alias.c alias_db.c alias_ftp.c alias_util.c \
	passwdauth.c sig.c
#CFLAGS+= -DHAVE_SHELL_CMD_WITH_ANY_MODE
CFLAGS += -Wall -DUSE_PERROR -DMSEXT -DPASSWDAUTH
LDADD += -lmd -lcrypt -lutil
DPADD += ${LIBMD} ${LIBCRYPT} ${LIBUTIL}
MAN8=	ppp.8
BINMODE=4555
BINOWN=	root

.include <bsd.prog.mk>
