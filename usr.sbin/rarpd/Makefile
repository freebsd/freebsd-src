#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.1 1994/08/08 01:03:57 wollman Exp $

PROG=	rarpd
MAN8=	rarpd.8
SRCS=	rarpd.c ether_addr.c
CFLAGS+= -DTFTP_DIR=\"/tftpboot\"

.include <bsd.prog.mk>
