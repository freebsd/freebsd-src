#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	Makefile,v 1.4 1995/05/30 05:47:41 rgrimes Exp

LIB=	telnet
SRCS=	genget.c getent.c misc.c
#SRCS+=	auth.c encrypt.c
#SRCS+=	kerberos.c enc_des.c
#SRCS+=	kerberos5.c
CFLAGS+= -DHAS_CGETENT

.include <bsd.lib.mk>

