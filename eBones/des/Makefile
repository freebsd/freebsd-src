#	@(#)Makefile	5.4 (Berkeley) 5/7/91
#	$Id: Makefile,v 1.1.1.1 1994/09/30 14:49:50 csgr Exp $

LIB=	des
SRCS=	cbc_cksm.c cbc_enc.c ecb_enc.c enc_read.c enc_writ.c pcbc_enc.c \
	qud_cksm.c rand_key.c read_pwd.c set_key.c str2key.c \
	cfb_enc.c 3ecb_enc.c ofb_enc.c 3cbc_enc.c
#MAN1=	des.1
#MAN3=	des.3

#LINKS=	crypt
CFLAGS+= -DDES_ENCRYPT -DKRBDES_ENCRYPT

# Kerberos 4?
#CFLAGS+=-DKRB4
#SRCS+=	kerberos.c

# Kerberos 5?
#CFLAGS+= -DKRB5
#SRCS+=	kerberos5.c

CFLAGS+= -I${.CURDIR}/include -DAUTHENTICATE
SHLIB_MAJOR?= 2
SHLIB_MINOR?= 0

.include "/usr/src/lib/Makefile.inc"
.include <bsd.lib.mk>
