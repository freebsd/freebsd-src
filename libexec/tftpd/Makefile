#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	tftpd
SRCS=	tftpd.c tftp-io.c tftp-utils.c tftp-file.c tftp-transfer.c tftp-options.c
WARNS=	3
WFORMAT=0
MAN=	tftpd.8
CFLAGS=-g -Wall
CFLAGS+=-I${.CURDIR}/../../usr.bin/tftp -I${.CURDIR}/../../libexec/tftpd
.PATH:	${.CURDIR}/../../usr.bin/tftp
COPTFLAGS = -O
LDFLAGS= -lwrap

.include <bsd.prog.mk>
