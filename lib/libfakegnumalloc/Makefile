#	$Id: Makefile,v 1.4 1996/09/02 13:15:00 phk Exp $

LIB=		fakegnumalloc
SHLIBDIR=	${LIBDIR}/compat
INTERNALLIB=	yes		# Do not build or install lib*.a
SHLIB_MAJOR=	2
SHLIB_MINOR=	0

SRCS=	fakegnumalloc.c

afterinstall:
	rm -f ${DESTDIR}${LIBDIR}/libgnumalloc*
	rm -f ${DESTDIR}${SHLIBDIR}/libgnumalloc*
	for i in ${DESTDIR}/var/db/pkg/gnumalloc* ; do \
		if [ -d $$i ] ; then exit 0 ; fi ; \
	done && \
	( cd ${DESTDIR}${SHLIBDIR} && \
	  ln -fs libfakegnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
		 libgnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} )

.include <bsd.lib.mk>
