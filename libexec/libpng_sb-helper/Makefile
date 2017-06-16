#
# $FreeBSD$
#
PROG=	libpng_sb-helper
SRCS=	libpng_sb-helper.c
INCS=	libpng_sb-helper.h

LDFLAGS+=	-Wl,-T,${SRCTOP}/lib/libc_cheri/sandbox.ld \
		-nostdlib
MAN=

CFLAGS+=        -I${SRCTOP}/contrib/libpng
CFLAGS+=        -I${SRCTOP}/lib/libpng

LIBADD=png z c_cheri
NEED_CHERI=	sandbox
WANT_DUMP=	yes

NO_SHARED=	yes

# don't know how to handle processor specific section `.preinit_array'
CHERI_LLD_BROKEN=yes

.include <bsd.prog.mk>
