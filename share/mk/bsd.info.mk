# $Id: bsd.info.mk,v 1.13.4.1 1995/09/17 11:14:36 davidg Exp $

BINMODE=        444
BINDIR?=	/usr/share/info
MAKEINFO?=	makeinfo
MAKEINFOFLAGS?=	--no-split # simplify some things, e.g., compression

.MAIN: all

.SUFFIXES: .gz .info .texi .texinfo
.texi.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} ${.IMPSRC} -o ${.TARGET}
.texinfo.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} ${.IMPSRC} -o ${.TARGET}

.PATH: ${.CURDIR}

.if !defined(NOINFOCOMPRESS)
IFILES=	${INFO:S/$/.info.gz/g}
all: ${IFILES}
.else
IFILES=	${INFO:S/$/.info/g}
all: ${IFILES}
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
distribute:     
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${DISTRIBUTION} SHARED=copies     
.endif

.if defined(SRCS)
${INFO}.info: ${SRCS}
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} ${SRCS:S/^/${.CURDIR}\//g} -o ${INFO}.info
.endif

depend:
	@echo -n

.if !target(obj)
.if defined(NOOBJ)
obj:
.else
obj:
	@cd ${.CURDIR}; rm -f obj; \
	here=`pwd`; dest=/usr/obj`echo $$here | sed 's,^/usr/src,,'`; \
	${ECHO} "$$here -> $$dest"; ln -s $$dest obj; \
	if test -d /usr/obj -a ! -d $$dest; then \
		mkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

clean:
	rm -f ${INFO:S/$/.info*/g} [eE]rrs mklog ${CLEANFILES}

cleandir: clean
	cd ${.CURDIR}; rm -rf obj

install:
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
maninstall:

.endif
