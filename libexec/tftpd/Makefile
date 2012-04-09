#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/tftpd/Makefile,v 1.10.2.4.4.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	tftpd
MAN=	tftpd.8
SRCS=	tftp-file.c tftp-io.c tftp-options.c tftp-transfer.c tftp-utils.c
SRCS+=	tftpd.c
WARNS=	6
WFORMAT=0
LDFLAGS= -lwrap

.include <bsd.prog.mk>
