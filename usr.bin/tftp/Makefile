#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.PATH: ${.CURDIR}/../../libexec/tftpd

PROG=	tftp
SRCS=	main.c tftp-file.c tftp-io.c tftp-options.c tftp-transfer.c
SRCS+=	tftp-utils.c tftp.c
CFLAGS+=-I${.CURDIR}/../../libexec/tftpd
DPADD=	${LIBEDIT} ${LIBTERMCAP}
LDADD=	-ledit -ltermcap

.include <bsd.prog.mk>
