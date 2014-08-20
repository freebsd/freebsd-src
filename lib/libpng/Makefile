# $FreeBSD$

LIB=	png
SHLIB_MAJOR= 15

CFLAGS+=	-I${.CURDIR}

.PATH: ${.CURDIR}/../../contrib/libpng
SRCS=	\
	png.c \
	pngerror.c \
	pngget.c \
	pngmem.c \
	pngpread.c \
	pngread.c \
	pngrio.c \
	pngrtran.c \
	pngrutil.c \
	pngset.c \
	pngtrans.c \
	pngwio.c \
	pngwrite.c \
	pngwtran.c \
	pngwutil.c

INCS=	\
	png.h \
	pngconf.h \
	pnglibconf.h

#INCSDIR=        ${INCLUDEDIR}/libpng15

WARNS?=	6

USE_CHERI=yes

.include <src.opts.mk>
.include <bsd.lib.mk>

CFLAGS+=	-Wno-cast-align \
		-Wno-tautological-constant-out-of-range-compare
