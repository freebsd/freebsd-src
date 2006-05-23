# $FreeBSD: src/libexec/telnetd/Makefile,v 1.24 2004/08/06 07:27:05 cperciva Exp $

# Do not define -DKLUDGELINEMODE, as it does not interact well with many
# telnet implementations.

TELNETDIR=	${.CURDIR}/../../contrib/telnet
.PATH:		${TELNETDIR}/telnetd

PROG=		telnetd
MAN=		telnetd.8

SRCS=		global.c slc.c state.c sys_term.c telnetd.c \
		termstat.c utility.c

CFLAGS+=	-DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOLD_ENVIRON \
		-DENV_HACK \
		-I${TELNETDIR} -DINET6

WARNS?=		2
WFORMAT?=	0

.if exists(${.OBJDIR}/../../lib/libtelnet)
LIBTELNET=	${.OBJDIR}/../../lib/libtelnet/libtelnet.a
.else
LIBTELNET=	${.CURDIR}/../../lib/libtelnet/libtelnet.a
.endif

DPADD=		${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET}
LDADD=		-lutil -ltermcap ${LIBTELNET}

.if !defined(RELEASE_CRUNCH)
.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
SRCS+=		authenc.c
CFLAGS+=	-DAUTHENTICATION -DENCRYPTION
DPADD+=		${LIBMP} ${LIBCRYPTO} ${LIBCRYPT} ${LIBPAM}
LDADD+=		-lmp -lcrypto -lcrypt ${MINUSLPAM}
.if !defined(NO_KERBEROS)
CFLAGS+=	-DKRB5 -DFORWARD -Dnet_write=telnet_net_write
DPADD+=		${LIBKRB5} ${LIBASN1} ${LIBROKEN} ${LIBCOM_ERR}
LDADD+=		-lkrb5 -lasn1 -lroken -lcom_err
.endif
.endif
.endif

.include <bsd.prog.mk>
