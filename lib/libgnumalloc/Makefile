#	$Id$

LIB=	fakegnumalloc
SRCS=	fakegnumalloc.c
NOMAN=	1

afterinstall:
	rm -f ${DESTDIR}${LIBDIR}/libfakegnumalloc*.a
	for i in ${DESTDIR}/var/db/pkg/gnumalloc* ; \
	do if [ -d $$i ] ; then exit 0 ; fi ; done && \
	( cd ${DESTDIR}${LIBDIR} && \
	rm -rf libgnumalloc* && \
	ln -fs libfakegnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	libgnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR} )

.include <bsd.lib.mk>
