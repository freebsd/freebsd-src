#	from: @(#)bsd.man.mk	5.2 (Berkeley) 5/11/90
#	$Id: bsd.man.mk,v 1.6 1994/06/05 20:42:39 csgr Exp $

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

INSTALL?=	install
MANGRP?=	bin
MANOWN?=	bin
MANMODE?=	444

MANDIR?=	/usr/share/man/man
MANSRC?=	${.CURDIR}
MINSTALL=	${INSTALL}  ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

MCOMPRESS=	gzip -f
BASENAME=	basename
ZEXTENSION=	.gz
.if !defined(NOMANCOMPRESS)
ZEXT=		${ZEXTENSION}
.else
ZEXT=
.endif

MANALL=		${MAN1} ${MAN2} ${MAN3} ${MAN3F} ${MAN4} ${MAN5}	\
		${MAN6} ${MAN7} ${MAN8}

maninstall: ${MANDEPEND}
.if defined(MAN1) && !empty(MAN1)
	(cd ${MANSRC}; ${MINSTALL} ${MAN1} ${DESTDIR}${MANDIR}1${MANSUBDIR})
.endif
.if defined(MAN2) && !empty(MAN2)
	(cd ${MANSRC}; ${MINSTALL} ${MAN2} ${DESTDIR}${MANDIR}2${MANSUBDIR})
.endif
.if defined(MAN3) && !empty(MAN3)
	(cd ${MANSRC}; ${MINSTALL} ${MAN3} ${DESTDIR}${MANDIR}3${MANSUBDIR})
.endif
.if defined(MAN3F) && !empty(MAN3F)
	(cd ${MANSRC}; ${MINSTALL} ${MAN3F} ${DESTDIR}${MANDIR}3f${MANSUBDIR})
.endif
.if defined(MAN4) && !empty(MAN4)
	(cd ${MANSRC}; ${MINSTALL} ${MAN4} ${DESTDIR}${MANDIR}4${MANSUBDIR})
.endif
.if defined(MAN5) && !empty(MAN5)
	(cd ${MANSRC}; ${MINSTALL} ${MAN5} ${DESTDIR}${MANDIR}5${MANSUBDIR})
.endif
.if defined(MAN6) && !empty(MAN6)
	(cd ${MANSRC}; ${MINSTALL} ${MAN6} ${DESTDIR}${MANDIR}6${MANSUBDIR})
.endif
.if defined(MAN7) && !empty(MAN7)
	(cd ${MANSRC}; ${MINSTALL} ${MAN7} ${DESTDIR}${MANDIR}7${MANSUBDIR})
.endif
.if defined(MAN8) && !empty(MAN8)
	(cd ${MANSRC}; ${MINSTALL} ${MAN8} ${DESTDIR}${MANDIR}8${MANSUBDIR})
.endif

# by default all pages are compressed
# we don't handle .so's yet
.if !empty(MANALL:S/ //g)
.if !defined(NOMANCOMPRESS) 
	@set ${MANALL} ;						\
	while test $$# -ge 1; do					\
		name=`${BASENAME} $$1`;					\
		sect=`expr $$name : '.*\.\([^.]*\)'`;			\
		echo "compressing in"					\
			"${DESTDIR}${MANDIR}$${sect}${MANSUBDIR}:"	\
			"$$name -> $${name}${ZEXT}";			\
		${MCOMPRESS} ${DESTDIR}${MANDIR}$${sect}${MANSUBDIR}/$$name ; \
		shift ;							\
	done ; true
.else
# we are installing uncompressed pages, so nuke any compressed pages
	@set ${MANALL} ;						\
	while test $$# -ge 1; do					\
		name=`${BASENAME} $$1`;					\
		sect=`expr $$name : '.*\.\([^.]*\)'`;			\
		rm -f ${DESTDIR}${MANDIR}$${sect}${MANSUBDIR}/$$name${ZEXTENSION};\
		shift ;							\
	done ; true
.endif
.endif

.if defined(MLINKS) && !empty(MLINKS)
	@set ${MLINKS}; \
	while test $$# -ge 2; do \
		name=$$1; \
		shift; \
		sect=`expr $$name : '.*\.\([^.]*\)'`; \
		dir=${DESTDIR}${MANDIR}$$sect; \
		l=$${dir}${MANSUBDIR}/$$name; \
		name=$$1; \
		shift; \
		sect=`expr $$name : '.*\.\([^.]*\)'`; \
		dir=${DESTDIR}${MANDIR}$$sect; \
		t=$${dir}${MANSUBDIR}/$$name; \
		echo $${t}${ZEXT} -\> $${l}${ZEXT}; \
		rm -f $${t}${ZEXTENSION}; \
		rm -f $${t}; \
		ln $${l}${ZEXT} $${t}${ZEXT}; \
	done; true
.endif
