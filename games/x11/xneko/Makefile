#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	xneko
MAN6=	xneko.6
DPADD+=		${X11BASE}/lib/libX11.a ${LIBM}
LDDESTDIR+=	-L${X11BASE}/lib
LDADD+=		-lX11 -lm

.include <bsd.prog.mk>
