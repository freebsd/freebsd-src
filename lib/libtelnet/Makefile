#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$Id: Makefile,v 1.2 1994/08/05 02:02:36 wollman Exp $

LIB=	telnet
SRCS=	genget.c getent.c misc.c
#SRCS+=	auth.c encrypt.c 
#SRCS+=	kerberos.c enc_des.c
#SRCS+=	kerberos5.c
CFLAGS+= -DHAS_CGETENT

.include <bsd.lib.mk>

