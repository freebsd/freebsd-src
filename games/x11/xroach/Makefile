#	@(#)Makefile	8.1 (Berkeley) 6/8/93

PROG=	xroach
MAN6=	xroach.6
CFLAGS+= -I${.CURDIR}/bitmaps
DPADD+=		${X11BASE}/lib/libX11.a ${LIBM}
LDDESTDIR+=	-L${X11BASE}/lib
LDADD+=		-lX11 -lm

.include <bsd.prog.mk>
