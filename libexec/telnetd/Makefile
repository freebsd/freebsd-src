#	@(#)Makefile	8.2 (Berkeley) 12/15/93
#	$Id: Makefile,v 1.8 1996/09/22 21:55:29 wosch Exp $

PROG=	telnetd
CFLAGS+=-DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS
#CFLAGS+=-DKLUDGELINEMODE
CFLAGS+=-DOLD_ENVIRON -DENV_HACK
CFLAGS+=-I${.CURDIR}/../../lib
SRCS=	global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c
DPADD=	${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET}
LDADD=	-lutil -ltermcap -ltelnet
MAN8=	telnetd.8

.include <bsd.prog.mk>
