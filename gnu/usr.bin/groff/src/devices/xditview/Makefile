# $FreeBSD: src/gnu/usr.bin/groff/src/devices/xditview/Makefile,v 1.2.14.1 2010/02/10 00:26:20 kensmith Exp $

PROG=		gxditview
SRCS=		device.c draw.c Dvi.c font.c lex.c page.c parse.c xditview.c
DPADD=		${LIBM}
LDADD=		-lm
CLEANFILES=	${MAN}

GENHDRS=	GXditview-ad.h
SRCS+=		${GENHDRS}
CFLAGS+=	-I.
CLEANFILES+=	${GENHDRS}

GXditview-ad.h: GXditview.ad
	${SHELL} ${DIST_DIR}/ad2c ${.ALLSRC} >${.TARGET}

# libxutil
SRCS+=		DviChar.c XFontName.c xmalloc.c

# X11 glue
CFLAGS+=	-I${X11BASE}/include
LDFLAGS=	-L${X11BASE}/lib
DPADD+=		${X11BASE}/lib/libXaw.a
LDADD+=		-lXaw

BINDIR=		${X11BASE}/bin
MANDIR=		${X11BASE}/man/man
FILESDIR=	${X11BASE}/lib/X11/app-defaults
FILES=		GXditview.ad
FILESNAME=	GXditview

.include <bsd.prog.mk>

.PATH: ${GROFF_DIST}/src/libs/libxutil
