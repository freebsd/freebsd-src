#	$Id: Makefile,v 1.5 1996/09/19 09:36:43 peter Exp $

LIB=		fakegnumalloc
SHLIBDIR=	${LIBDIR}/compat
INTERNALLIB=	yes		# Do not build or install lib*.a
SHLIB_MAJOR=	2
SHLIB_MINOR=	0

SRCS=	fakegnumalloc.c

afterinstall:
	for i in ${DESTDIR}/var/db/pkg/gnumalloc* ; do \
		if [ -d $$i ] ; then exit 0 ; fi ; \
	done && \
	( cd ${DESTDIR}${SHLIBDIR} && \
	  rm -f libgnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} && \
	  ln -fs libfakegnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
		 libgnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} )

.include <bsd.lib.mk>
