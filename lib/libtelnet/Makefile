#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$Id: Makefile,v 1.3 1994/08/12 22:41:25 csgr Exp $

LIB=	telnet
SRCS=	genget.c getent.c misc.c
#SRCS+=	auth.c encrypt.c
#SRCS+=	kerberos.c enc_des.c
#SRCS+=	kerberos5.c
CFLAGS+= -DHAS_CGETENT

.include <bsd.lib.mk>

