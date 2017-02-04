#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	tftpd
MAN=	tftpd.8
SRCS=	tftp-file.c tftp-io.c tftp-options.c tftp-transfer.c tftp-utils.c
SRCS+=	tftpd.c
WFORMAT=0

.if ${MK_TCP_WRAPPERS} != "no"
CFLAGS+=	-DLIBWRAP
DPADD+=		${LIBWRAP}
LDADD+=		-lwrap
.endif

.include <bsd.prog.mk>
