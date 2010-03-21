#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/talk/Makefile,v 1.10.38.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	talk
SRCS=	ctl.c ctl_transact.c display.c get_addrs.c get_iface.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>
