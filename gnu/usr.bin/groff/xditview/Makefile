.if exists(${X11BASE}/include/X11/Xlib.h)
.if exists(${X11BASE}/lib)
.if exists(${DESTDIR}${X11BASE}/bin)
.if exists(${DESTDIR}${X11BASE}/man/man1)
.if exists(${DESTDIR}${X11BASE}/lib/X11/app-defaults)

BINDIR=		${X11BASE}/bin
MANDIR=		${X11BASE}/man/man

MANDEPEND=	gxditview.1
CLEANFILES+=	${MANDEPEND}

PROG=		gxditview
CFLAGS+=	-I${X11BASE}/include -DFONTPATH=\"$(fontpath)\"
SRCS+=		xditview.c Dvi.c draw.c font.c lex.c page.c parse.c \
		XFontName.c DviChar.c device.c
LDDESTDIR+=	-L${X11BASE}/lib
LDADD+=		-lXaw -lXmu -lXt -lSM -lICE -lXext -lX11 -lm
DPADD+=		${X11BASE}/lib/libXaw.a ${X11BASE}/lib/libXmu.a \
		${X11BASE}/lib/libXt.a ${X11BASE}/lib/libSM.a \
		${X11BASE}/lib/libICE.a ${X11BASE}/lib/libXext.a \
		${X11BASE}/lib/libX11.a

afterinstall:
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/GXditview.ad \
		${DESTDIR}${X11BASE}/lib/X11/app-defaults/GXditview
.endif
.endif
.endif
.endif
.endif

.include "../Makefile.cfg"
.include <bsd.prog.mk>
