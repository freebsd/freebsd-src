# $Id: bsd.info.mk,v 1.17 1996/03/07 23:39:45 wosch Exp $

BINMODE=        444
BINDIR?=	/usr/share/info
MAKEINFO?=	makeinfo
MAKEINFOFLAGS+=	--no-split # simplify some things, e.g., compression

.MAIN: all

.SUFFIXES: .gz .info .texi .texinfo
.texi.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} ${.IMPSRC} -o ${.TARGET}
.texinfo.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} ${.IMPSRC} -o ${.TARGET}

.PATH: ${.CURDIR}

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
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} ${SRCS:S/^/${.CURDIR}\//g} -o ${INFO}.info
.endif

depend: _SUBDIR
	@echo -n

clean: _SUBDIR
	rm -f ${INFO:S/$/.info*/g} Errs errs mklog ${CLEANFILES}

install: _SUBDIR
	@if [ ! -d "${DESTDIR}${BINDIR}" ]; then \
		/bin/rm -f ${DESTDIR}${BINDIR}  ; \
		mkdir -p ${DESTDIR}${BINDIR}  ; \
		chown root.wheel ${DESTDIR}${BINDIR}  ; \
		chmod 755 ${DESTDIR}${BINDIR}  ; \
        else \
                true ; \
        fi
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${IFILES} ${DESTDIR}${BINDIR}

.if !target(maninstall)
maninstall: _SUBDIR
.endif

.include <bsd.dep.mk>
.include <bsd.obj.mk>
