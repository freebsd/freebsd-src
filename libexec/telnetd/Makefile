#	@(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD: src/libexec/telnetd/Makefile,v 1.11.2.1 1999/08/29 15:04:18 peter Exp $

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
