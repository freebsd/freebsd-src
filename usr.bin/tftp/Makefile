#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

CFLAGS=-g -Wall
WARNS=	3
PROG=	tftp
SRCS=	main.c tftp.c tftp-utils.c tftp-io.c tftp-file.c tftp-transfer.c tftp-options.c
DPADD=	${LIBEDIT} ${LIBTERMCAP}
LDADD=	-ledit -ltermcap
CFLAGS+=-I${.CURDIR}/../../libexec/tftpd -I${.CURDIR}/../../usr.bin/tftp
.PATH:  ${.CURDIR}/../../libexec/tftpd

.include <bsd.prog.mk>
