#	@(#)Makefile	5.13 (Berkeley) 3/1/91

PROG=	talk
DPADD=	${LIBCURSES} ${LIBTERMCAP}
LDADD=	-lcurses -ltermlib
SRCS=	ctl.c ctl_transact.c display.c get_addrs.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c

.include <bsd.prog.mk>
