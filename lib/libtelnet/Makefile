#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD$

.include <src.opts.mk>

# XXX Must be the same as in kerberos5/Makefile.inc
# XXX There will be a better way to reduce duplication but
# XXX for now this will do.
KRB5DIR=	${SRCTOP}/crypto/heimdal

PACKAGE=lib${LIB}
TELNETDIR=	${SRCTOP}/contrib/telnet
.PATH:		${TELNETDIR}/libtelnet

LIB=		telnet

INTERNALLIB=

SRCS=		genget.c getent.c misc.c
CFLAGS+=	-I${TELNETDIR}

WARNS?=		2

.if ${MK_OPENSSL} != "no"
SRCS+=		encrypt.c auth.c enc_des.c sra.c pk.c
CFLAGS+=	-DENCRYPTION -DAUTHENTICATION -DSRA
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
SRCS+=		kerberos5.c
CFLAGS+=	-DKRB5 -DFORWARD -Dnet_write=telnet_net_write
CFLAGS+= 	-I${SRCTOP}/kerberos5/include \
		-I${KRB5DIR}/lib/krb5 -I${.OBJDIR:H:H}/kerberos5/lib/libkrb5 \
		-I${KRB5DIR}/lib/asn1 -I${.OBJDIR:H:H}/kerberos5/lib/libasn1
.endif

.include <bsd.lib.mk>
