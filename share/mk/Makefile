#	@(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	bsd.doc.mk bsd.lib.mk bsd.man.mk bsd.prog.mk bsd.subdir.mk sys.mk
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINOWN} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

.include <bsd.prog.mk>
