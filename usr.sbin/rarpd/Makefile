#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.1.1.1 1995/03/02 06:41:39 wpaul Exp $

PROG=	rarpd
MAN8=	rarpd.8
SRCS=	rarpd.c arptab.c ether_addr.c
CFLAGS+= -DTFTP_DIR=\"/tftpboot\"

.include <bsd.prog.mk>
