#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	talk
DPADD=	${LIBCURSES} ${LIBTERMCAP}
LDADD=	-lcurses -ltermcap
CFLAGS+= -Wall -Wstrict-prototypes -Wno-unused
SRCS=	ctl.c ctl_transact.c display.c get_addrs.c get_iface.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c

.include <bsd.prog.mk>
