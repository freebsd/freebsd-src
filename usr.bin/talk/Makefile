#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/talk/Makefile,v 1.7 1999/08/28 01:06:10 peter Exp $

PROG=	talk
DPADD=	${LIBCURSES} ${LIBTERMCAP}
LDADD=	-lcurses -ltermcap
CFLAGS+= -Wall -Wstrict-prototypes -Wno-unused
SRCS=	ctl.c ctl_transact.c display.c get_addrs.c get_iface.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c

.include <bsd.prog.mk>
