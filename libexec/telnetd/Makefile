#	@(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD: src/libexec/telnetd/Makefile,v 1.13.2.1 2001/04/25 10:41:02 ru Exp $

PROG=	telnetd
CFLAGS+=-DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS
#CFLAGS+=-DKLUDGELINEMODE
CFLAGS+=-DOLD_ENVIRON -DENV_HACK
CFLAGS+=-I${.CURDIR}/../../lib
CFLAGS+=-DINET6
SRCS=	global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c
DPADD=	${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET}
LDADD=	-lutil -ltermcap -ltelnet
MAN=	telnetd.8

.include <bsd.prog.mk>
