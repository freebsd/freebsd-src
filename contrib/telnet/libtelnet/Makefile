#       From: @(#)Makefile      8.2 (Berkeley) 12/15/93
#       $Id: Makefile,v 1.1.1.1.6.5 1996/06/30 12:50:37 markm Exp $

INTERNALSTATICLIB=	YES

LIB=    telnet
SRCS=   encrypt.c genget.c getent.c misc.c

# SRA Authentication & DES Encryption
CFLAGS+= -I.. -DHAS_CGETENT -DENCRYPTION -DAUTHENTICATION -DSRA
CFLAGS+= -DDES_ENCRYPTION
LDADD+= -ldes -lmp
SRCS+= auth.c enc_des.c sra.c pk.c

#don't bother to install
install:
	/usr/bin/true

.include <bsd.lib.mk>
