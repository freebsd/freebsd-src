#       @(#)Makefile    8.2 (Berkeley) 12/15/93
#       $Id: Makefile,v 1.2.6.6 1996/06/30 12:50:55 markm Exp $

# Do not define -DKLUDGELINEMODE, as it does not interact well with many
# telnet implementations.

DISTRIBUTION=	des

PROG=   telnetd
MAN8=   telnetd.8

BINDIR=	/usr/libexec

SRCS=   authenc.c global.c slc.c state.c sys_term.c telnetd.c \
        termstat.c utility.c

DPADD=  ${.CURDIR}/../libtelnet.a ${LIBUTIL} ${LIBTERMCAP} ${LIBDES} ${LIBMP}
DPADD+= ${LIBCRYPT}
LDADD=  -L${.CURDIR}/../libtelnet -lutil -ltermcap -ltelnet -ldes -lmp -lcrypt

CFLAGS+= -DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOLD_ENVIRON -DENV_HACK
CFLAGS+= -DENCRYPTION -DAUTHENTICATION -I${.CURDIR}/../

.include <bsd.prog.mk>
