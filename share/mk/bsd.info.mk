# $Id: bsd.info.mk,v 1.23 1997/01/11 10:51:36 jmacd Exp $

MAKEINFO?=	makeinfo
MAKEINFOFLAGS+=	--no-split # simplify some things, e.g., compression
SRCDIR?=	${.CURDIR}
INFODIRFILE?=   dir
INFOTMPL?=      /usr/share/info/dir-tmpl
INSTALLINFO?=   install-info
INFOSECTION?=   Miscellaneous

.MAIN: all

.SUFFIXES: .gz .info .texi .texinfo
.texi.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${.IMPSRC} -o ${.TARGET}
.texinfo.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${.IMPSRC} -o ${.TARGET}

.PATH: ${.CURDIR} ${SRCDIR}

IFILENS= ${INFO:S/$/.info/g}

.if !defined(NOINFO)
.if !defined(NOINFOCOMPRESS)
IFILES=	${INFO:S/$/.info.gz/g}
all: ${IFILES} _SUBDIR
.else
IFILES=	${IFILENS}
all: ${IFILES} _SUBDIR
.endif
.else
all:
.endif

GZIPCMD?=	gzip

.for x in ${INFO:S/$/.info/g}
${x:S/$/.gz/}:	${x}
	${GZIPCMD} -c ${.ALLSRC} > ${.TARGET}
.endfor

.for x in ${INFO}
INSTALLINFODIRS+= ${x:S/$/-install/}
${x:S/$/-install/}:
	${INSTALLINFO} --defsection=${INFOSECTION} \
		       --defentry=${INFOENTRY_${x}} \
		       ${x}.info ${DESTDIR}/${BINDIR}/${INFODIRFILE}
.endfor

.PHONY: ${INSTALLINFODIRS}

# The default is "info" and it can never be "bin"
DISTRIBUTION?=	info
.if ${DISTRIBUTION} == "bin"
DISTRIBUTION=	info
.endif

.if !target(distribute)
distribute: _SUBDIR
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${DISTRIBUTION} SHARED=copies
.endif

.if defined(SRCS)
${INFO}.info: ${SRCS}
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${SRCS:S/^/${SRCDIR}\//g} -o ${INFO}.info
.endif

depend: _SUBDIR
	@echo -n

clean: _SUBDIR
	rm -f ${INFO:S/$/.info*/g} Errs errs mklog ${CLEANFILES}

.if !defined(NOINFO)
install: ${INSTALLINFODIRS} _SUBDIR
	${INSTALL} ${COPY} -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} \
		${IFILES} ${DESTDIR}${INFODIR}
.else
install:
.endif

.if !target(maninstall)
maninstall: _SUBDIR
.endif

.include <bsd.dep.mk>
.include <bsd.obj.mk>
