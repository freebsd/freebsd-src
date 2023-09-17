
PROG=	vtfontcvt
SRCS=	vtfontcvt.c lz4.c
MAN8=	vtfontcvt.8

# lz4 compression functionality
.PATH: ${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4

.include <bsd.prog.mk>
