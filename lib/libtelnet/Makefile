#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD: src/lib/libtelnet/Makefile,v 1.20 2004/08/06 07:27:04 cperciva Exp $

TELNETDIR=	${.CURDIR}/../../contrib/telnet
.PATH:		${TELNETDIR}/libtelnet

LIB=		telnet

INTERNALLIB=	yes

SRCS=		genget.c getent.c misc.c
CFLAGS+=	-I${TELNETDIR}

WARNS?=		0

.if !defined(RELEASE_CRUNCH)
.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
SRCS+=		encrypt.c auth.c enc_des.c sra.c pk.c
CFLAGS+=	-DENCRYPTION -DAUTHENTICATION -DSRA
.if !defined(NO_KERBEROS)
SRCS+=		kerberos5.c
CFLAGS+=	-DKRB5 -I${KRB5DIR}/lib/krb5 -I${KRB5OBJDIR} -I${ASN1OBJDIR}
CFLAGS+=	-DFORWARD -Dnet_write=telnet_net_write
.endif
.endif
.endif

INCS=		${TELNETDIR}/arpa/telnet.h
INCSDIR=	${INCLUDEDIR}/arpa

.include <bsd.lib.mk>
