#	@(#)Makefile	8.2 (Berkeley) 12/15/93
#	$Id$

# Do not define -DKLUDGELINEMODE, as it does not interact well with many
# telnet implementations.

PROG=	telnetd
MAN8=	telnetd.8

SRCS=	authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c

DPADD=	${TELNETOBJDIR}/libtelnet.a ${LIBUTIL} ${LIBTERMCAP}
LDADD=	-L${TELNETOBJDIR} -lutil -ltermcap -ltelnet

CFLAGS+= -DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOLD_ENVIRON -DENV_HACK
CFLAGS+= -DENCRYPTION -I${.CURDIR}/../../lib

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_EBONES))
CFLAGS+=-DAUTHENTICATION
LDADD+= -ldes -lkrb
DPADD+= ${LIBDES} ${LIBKRB}
.endif

# Used only in krb4encpwd.c and rsaencpwd.c (libtelnet), not yet active
#LDADD+= -ldescrypt

.include <bsd.prog.mk>
