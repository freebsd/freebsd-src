# $FreeBSD$

LIB=		vuln_png
SHLIB_MAJOR=	1

SRCS=	png.c		\
	pngerror.c	\
	pngget.c	\
	pngmem.c	\
	pngpread.c	\
	pngread.c	\
	pngrio.c	\
	pngrtran.c	\
	pngrutil.c	\
	pngset.c	\
	pngtrans.c	\
	pngwio.c	\
	pngwrite.c	\
	pngwtran.c	\
	pngwutil.c

INCS=	png.h		\
	pngconf.h

WARNS?=		0

CFLAGS+=	-DPNG_NO_FLOATING_POINT_SUPPORTED \
		-DPNG_NO_CONSOLE_IO \
		-DCTSRD_BACKDOOR

.include <bsd.lib.mk>
