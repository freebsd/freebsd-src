
.include <src.opts.mk>

.PATH: ${SRCTOP}/libexec/tftpd

PROG=	tftp
SRCS=	main.c tftp-file.c tftp-io.c tftp-options.c tftp-transfer.c
SRCS+=	tftp-utils.c tftp.c
CFLAGS+=-I${SRCTOP}/libexec/tftpd

LIBADD=	edit

HAS_TESTS=
SUBDIR.${MK_TESTS}=	tests

.include <bsd.prog.mk>
