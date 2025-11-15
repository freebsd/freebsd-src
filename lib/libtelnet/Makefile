.include <src.opts.mk>

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
CFLAGS+=	-DOPENSSL_API_COMPAT=0x10100000L
.endif

.if ${MK_KERBEROS_SUPPORT} != "no" && ${MK_MITKRB5} != "yes"
# MIT KRB5 deprecated and removed 3DES. Therefore no telnet support.
SRCS+=		kerberos5.c
CFLAGS+=	-DKRB5 -DFORWARD -Dnet_write=telnet_net_write
.endif

.include <bsd.lib.mk>
