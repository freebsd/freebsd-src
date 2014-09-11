# $FreeBSD$

LIB?=	png
SHLIB_MAJOR= 15

BUILDDIR?=	${.CURDIR}
SRCDIR?=	${.CURDIR}/../../contrib/libpng

CFLAGS+=	-I${BUILDDIR}

.PATH: ${SRCDIR}
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

.if ${BUILDDIR} == ${.CURDIR}
INCS=	\
	png.h \
	pngconf.h \
	pnglibconf.h
.endif

WARNS?=	6

USE_CHERI=yes

CFLAGS+=	${CFLAGS.${.IMPSRC:T}}
.include <src.opts.mk>
.include <bsd.lib.mk>

CFLAGS:=	${CFLAGS:S/-O/-O2/}
CFLAGS.pngerror.c=	-O0

CFLAGS+=	-Wno-cast-align \
		-Wno-tautological-constant-out-of-range-compare
