#	@(#)Makefile	8.2 (Berkeley) 12/15/93

PROG=	telnetd
CFLAGS+=-DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS
CFLAGS+=-DOLD_ENVIRON -DENV_HACK
CFLAGS+=-I${.CURDIR}/../../lib
#CFLAGS+=-DAUTHENTICATION -DENCRYPTION
SRCS=	global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c
#SRCS+=	authenc.c
DPADD=	${LIBUTIL} ${LIBTERM}
LDADD=	-lutil -ltermcap -ltelnet
#LDADD+=	-lkrb -ldes
MAN8=	telnetd.8


.include <bsd.prog.mk>
