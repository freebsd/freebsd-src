#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/tftpd/Makefile,v 1.9.24.4.2.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	tftpd
MAN=	tftpd.8
SRCS=	tftp-file.c tftp-io.c tftp-options.c tftp-transfer.c tftp-utils.c
SRCS+=	tftpd.c
WARNS=	6
WFORMAT=0
LDFLAGS= -lwrap

.include <bsd.prog.mk>
