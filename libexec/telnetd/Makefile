#	@(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD$

PROG=	telnetd
CFLAGS+=-DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS
#CFLAGS+=-DKLUDGELINEMODE
CFLAGS+=-DOLD_ENVIRON -DENV_HACK
CFLAGS+=-I${.CURDIR}/../../lib
CFLAGS+=-DINET6
SRCS=	global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c

.if exists(${.OBJDIR}/../../lib/libtelnet)
LIBTELNET=	${.OBJDIR}/../../lib/libtelnet/libtelnet.a
.else
LIBTELNET=	${.CURDIR}/../../lib/libtelnet/libtelnet.a
.endif

DPADD=	${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET}
LDADD=	-lutil -ltermcap ${LIBTELNET}
MAN=	telnetd.8

.include <bsd.prog.mk>
