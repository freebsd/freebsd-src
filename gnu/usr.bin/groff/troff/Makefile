PROG=		troff
SRCS=		env.cc node.cc input.cc div.cc symbol.cc dictionary.cc reg.cc \
		number.cc majorminor.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}
MANDEPEND=	troff.1

CLEANFILES+=	majorminor.cc ${MANDEPEND}

majorminor.cc: ${.CURDIR}/../VERSION
	@${ECHO} Making $@
	@-rm -f $@
	@echo const char \*major_version = \
	\"`sed -e 's/^\([^.]*\)\..*$$/\1/' ${.CURDIR}/../VERSION`\"\; >$@
	@echo const char \*minor_version = \
	\"`sed -e 's/^[^.]*\.\([0-9]*\).*$$/\1/' ${.CURDIR}/../VERSION`\"\; >>$@

afterinstall:
	${INSTALL} -c -o bin -g bin -m 444 ${.CURDIR}/hyphen.us \
		${DESTDIR}${tmacdir}/hyphen.us
	${INSTALL} -c -o bin -g bin -m 444 ${.CURDIR}/hyphen.us-ru \
		${DESTDIR}${tmacdir}/hyphen.us-ru

.include "../Makefile.cfg"
.include <bsd.prog.mk>
