#	from: @(#)bsd.doc.mk	5.3 (Berkeley) 1/2/91
#	$Id: bsd.doc.mk,v 1.7 1994/04/19 17:15:55 jkh Exp $

PRINTER?=	ps

BIB?=		bib
EQN?=		eqn -T${PRINTER}
GREMLIN?=	grn
GRIND?=		vgrind -f
INDXBIB?=	indxbib
INSTALL?=	install
PIC?=		pic
REFER?=		refer
ROFF?=		groff -T${PRINTER} ${MACROS} -o${PAGES}
SOELIM?=	soelim
TBL?=		tbl

PAGES?=		1-

# Compatibility mode flag for groff.  Use this when formatting documents with
# Berkeley me macros.
COMPAT?=	-C

.PATH: ${.CURDIR}

all:	${DOC}.${PRINTER}

.if !target(print)
print: ${DOC}.${PRINTER}
	lpr -P${PRINTER} ${DOC}.${PRINTER}
.endif

.if !target(obj)
.if defined(NOOBJ)
obj:
.else
obj:
	@cd ${.CURDIR}; rm -f obj > /dev/null 2>&1 || true; \
	here=`pwd`; subdir=`echo $$here | sed 's,^/usr/src/,,'`; \
	if test $$here != $$subdir ; then \
		dest=/usr/obj/$$subdir ; \
		echo "$$here -> $$dest"; ln -s $$dest obj; \
		if test -d /usr/obj -a ! -d $$dest; then \
			mkdir -p $$dest; \
		else \
			true; \
		fi; \
	else \
		true ; \
		dest=$$here/obj ; \
		echo "making $$here/obj" ; \
		if test ! -d obj ; then \
			mkdir $$here/obj; \
		fi ; \
	fi;
.endif
.endif

clean:
	rm -f ${DOC}.* [eE]rrs mklog ${CLEANFILES}

cleandir:
	rm -f ${DOC}.* [eE]rrs mklog ${CLEANFILES}
	cd ${.CURDIR}; rm -rf obj

FILES?=	${SRCS}
install:
	@if [ ! -d "${DESTDIR}${BINDIR}/${VOLUME}" ]; then \
                /bin/rm -f ${DESTDIR}${BINDIR}/${VOLUME}  ; \
                mkdir -p ${DESTDIR}${BINDIR}/${VOLUME}  ; \
                chown root.wheel ${DESTDIR}${BINDIR}/${VOLUME}  ; \
                chmod 755 ${DESTDIR}${BINDIR}/${VOLUME}  ; \
        else \
                true ; \
        fi
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${DOC}.* \
	${DESTDIR}${BINDIR}/${VOLUME}

spell: ${SRCS}
	(cd ${.CURDIR};  spell ${SRCS} ) | sort | \
		comm -23 - ${.CURDIR}/spell.ok > ${DOC}.spell

BINDIR?=	/usr/share/doc
BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	444
