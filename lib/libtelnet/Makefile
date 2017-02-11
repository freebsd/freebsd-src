#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD$

.include <bsd.own.mk>

TELNETDIR=	${SRCTOP}/contrib/telnet
.PATH:		${TELNETDIR}/libtelnet

LIB=		telnet

INTERNALLIB=

SRCS=		genget.c getent.c misc.c
CFLAGS+=	-I${TELNETDIR}

WARNS?=		2

.if !defined(RELEASE_CRUNCH)
.if ${MK_OPENSSL} != "no"
SRCS+=		encrypt.c auth.c enc_des.c sra.c pk.c
CFLAGS+=	-DENCRYPTION -DAUTHENTICATION -DSRA
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
SRCS+=		kerberos5.c
CFLAGS+=	-DKRB5 -I${KRB5DIR}/lib/krb5 -I${KRB5OBJDIR} -I${ASN1OBJDIR}
CFLAGS+=	-DFORWARD -Dnet_write=telnet_net_write
.endif
.endif

INCS=		${TELNETDIR}/arpa/telnet.h
INCSDIR=	${INCLUDEDIR}/arpa

.include <bsd.lib.mk>
