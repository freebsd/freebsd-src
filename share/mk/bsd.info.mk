# $Id: bsd.info.mk,v 1.9 1995/01/23 20:53:38 jkh Exp $

BINMODE=        444
BINDIR?=	/usr/share/info
MAKEINFO?=	makeinfo
MAKEINFOFLAGS?=	# --no-split would simplify some things, e.g., compression

.MAIN: all

.SUFFIXES: .info .texi .texinfo
.texi.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} ${.IMPSRC} -o ${.TARGET}
.texinfo.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} ${.IMPSRC} -o ${.TARGET}

.PATH: ${.CURDIR}

all: ${INFO:S/$/.info/g}

DISTRIBUTION?=	info
.if !target(distribute)
distribute:     
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${RELEASEDIR}/${DISTRIBUTION} SHARED=copies     
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
		${INFO:S/$/.info*/g} ${DESTDIR}${BINDIR}

