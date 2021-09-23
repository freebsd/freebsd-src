# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	telnet

TELNETDIR=	${SRCTOP}/contrib/telnet
.PATH:		${TELNETDIR}/telnet

PROG=		telnet

SRCS=		commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c utilities.c

CFLAGS+=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DOPIE \
		-I${TELNETDIR} -I${TELNETDIR}/libtelnet/

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=	-DINET6
.endif

WARNS?=		2

LIBADD=		telnet tinfow

CFLAGS+=	-DIPSEC
LIBADD+=	ipsec

.if ${MK_OPENSSL} != "no"
SRCS+=		authenc.c
CFLAGS+=	-DENCRYPTION -DAUTHENTICATION -DIPSEC
LIBADD+=	mp crypto ipsec pam
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+=	-DKRB5 -DFORWARD -Dnet_write=telnet_net_write
LIBADD+=	krb5 roken
.endif

.include <bsd.prog.mk>
