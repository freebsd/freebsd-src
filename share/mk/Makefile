#	@(#)Makefile	5.3 (Berkeley) 6/22/90

FILES=	bsd.README bsd.doc.mk bsd.lib.mk bsd.man.mk bsd.own.mk \
	bsd.prog.mk bsd.subdir.mk sys.mk
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINOWN} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

.include <bsd.prog.mk>
