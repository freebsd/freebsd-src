#
# $FreeBSD$
#
PROG=	libpng_sb-helper
SRCS=	libpng_sb-helper.c

.PATH: ${.CURDIR}/../../contrib/libpng
CFLAGS+=	-DPNG_DEBUG=4
SRCS+=	png.c \
	pngerror.c \
	pngget.c \
	pngmem.c \
	pngpread.c \
	pngread.c \
	pngrio.c \
	pngrtran.c \
	pngrutil.c \
	pngset.c \
	pngtrans.c

.PATH: ${.CURDIR}/../../lib/libz
SRCS+=	adler32.c \
	crc32.c \
	inffast.c \
	inflate.c \
	inftrees.c \
	zutil.c

INCS=	libpng_sb-helper.h

LDFLAGS+=	-Wl,--script=${.CURDIR}/../../lib/libc_cheri/sandbox.ld \
		-nostdlib
MAN=
DPADD=	${LIBC_CHERI}
LDADD=	-lc_cheri

USE_CHERI=	yes
USE_CHERI_STACK=	yes

NO_SHARED=	yes

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		libpng_sb-helper.bin libpng_sb-helper.dump
CLEANFILES=	libpng_sb-helper.bin libpng_sb-helper.dump

libpng_sb-helper.bin: libpng_sb-helper
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}

libpng_sb-helper.dump: libpng_sb-helper
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>

CFLAGS+=	${CFLAGS.${.IMPSRC:T}}

CFLAGS:=	${CFLAGS:S/-O/-O2/}
CFLAGS.pngerror.c=	-O0
CFLAGS.pngrutil.c=	-Wno-cast-align
CFLAGS.pngset.c=	-Wno-tautological-constant-out-of-range-compare
CFLAGS.zutil.c=		-Wno-incompatible-pointer-types-discards-qualifiers
#CFLAGS.inflate.c=	-O0
CFLAGS.inffast.c=	-O0
