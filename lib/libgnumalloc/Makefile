#	$Id: Makefile,v 1.2 1996/02/12 12:40:04 phk Exp $

LIB=	fakegnumalloc
SHLIB_MAJOR=	2
SHLIB_MINOR=	0
SRCS=	fakegnumalloc.c
NOMAN=	1

afterinstall:
	rm -f ${DESTDIR}${LIBDIR}/libfakegnumalloc*.a
	for i in ${DESTDIR}/var/db/pkg/gnumalloc* ; do \
		if [ -d $$i ] ; then exit 0 ; fi ; \
	done && \
	( cd ${DESTDIR}${SHLIBDIR} && \
	  rm -f libgnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} && \
	  ln -fs libfakegnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
		 libgnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} )

.include <bsd.lib.mk>
