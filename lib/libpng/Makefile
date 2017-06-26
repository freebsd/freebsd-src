# $FreeBSD$

LIB?=	png
SHLIB_MAJOR= 15
PNG_PRIVATE?=yes
.if ${PNG_PRIVATE} == "yes"
PRIVATELIB=
.endif

BUILDDIR?=	${.CURDIR}
SRCDIR?=	${SRCTOP}/contrib/libpng

CFLAGS+=	-I${BUILDDIR}

.PATH: ${SRCDIR}
SRCS+=	\
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

LIBADD=	z

WARNS?=	6

WANT_CHERI=	yes

CFLAGS+=	${CFLAGS.${.IMPSRC:T}}
.include <bsd.lib.mk>

CFLAGS.pngerror.c=	-O0

CFLAGS+=	-Wno-cast-align

.if ${COMPILER_TYPE} == "clang"
CFLAGS+=	-Wno-tautological-constant-out-of-range-compare \
		-Wno-cast-qual
.endif

.if ${COMPILER_TYPE} == "gcc"
CFLAGS+=	-Wno-error
.endif
