# $FreeBSD$

PROG=		telnet

CFLAGS+=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DOPIE \
		-I${.CURDIR}/../../lib

SRCS=		commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c tn3270.c utilities.c

.if exists(${.OBJDIR}/../../lib/libtelnet)
LIBTELNET=	${.OBJDIR}/../../lib/libtelnet/libtelnet.a
.else
LIBTELNET=	${.CURDIR}/../../lib/libtelnet/libtelnet.a
.endif

DPADD=		${LIBTERMCAP} ${LIBTELNET}
LDADD=		-ltermcap ${LIBTELNET}

.if !defined(RELEASE_CRUNCH)
CFLAGS+=	-DINET6 -DIPSEC
DPADD+=		${LIBIPSEC}
LDADD+=		-lipsec
.else
.PATH:		${.CURDIR}/../../lib/libtelnet
# XXX This was copied from lib/libtelnet/Makefile.
SRCS+=		genget.c getent.c misc.c
CFLAGS+=	-DHAS_CGETENT
.endif

.include <bsd.prog.mk>
