# $Id: bsd.info.mk,v 1.19 1996/09/03 15:14:45 bde Exp $

MAKEINFO?=	makeinfo
MAKEINFOFLAGS+=	--no-split # simplify some things, e.g., compression
SRCDIR?=	${.CURDIR}

.MAIN: all

.SUFFIXES: .gz .info .texi .texinfo
.texi.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${.IMPSRC} -o ${.TARGET}
.texinfo.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${.IMPSRC} -o ${.TARGET}

.PATH: ${.CURDIR} ${SRCDIR}

.if !defined(NOINFOCOMPRESS)
IFILES=	${INFO:S/$/.info.gz/g}
all: ${IFILES} _SUBDIR
.else
IFILES=	${INFO:S/$/.info/g}
all: ${IFILES} _SUBDIR
.endif

GZIPCMD?=	gzip

.for x in ${INFO:S/$/.info/g}
${x:S/$/.gz/}:	${x}
	${GZIPCMD} -c ${.ALLSRC} > ${.TARGET}
.endfor

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
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${SRCS:S/^/${.SRCDIR}\//g} -o ${INFO}.info
.endif

depend: _SUBDIR
	@echo -n

clean: _SUBDIR
	rm -f ${INFO:S/$/.info*/g} Errs errs mklog ${CLEANFILES}

install: _SUBDIR
	${INSTALL} ${COPY} -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} \
		${IFILES} ${DESTDIR}${INFODIR}

.if !target(maninstall)
maninstall: _SUBDIR
.endif

.include <bsd.dep.mk>
.include <bsd.obj.mk>
