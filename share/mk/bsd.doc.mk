#	from: @(#)bsd.doc.mk	5.3 (Berkeley) 1/2/91
#	$Id: bsd.doc.mk,v 1.5 1993/10/11 03:48:05 rgrimes Exp $

PRINTER?=	ps

BIB?=		bib
EQN?=		eqn -T${PRINTER}
GREMLIN?=	grn
GRIND?=		vgrind -f
INDXBIB?=	indxbib
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

clean cleandir:
	rm -f ${DOC}.* [eE]rrs mklog ${CLEANFILES}
	rm -rf obj

FILES?=	${SRCS}
install:
	@if [ ! -d "${DESTDIR}${BINDIR}/${DIR}" ]; then \
                /bin/rm -f ${DESTDIR}${BINDIR}/${DIR}  ; \
                mkdir -p ${DESTDIR}${BINDIR}/${DIR}  ; \
                chown root.wheel ${DESTDIR}${BINDIR}/${DIR}  ; \
                chmod 755 ${DESTDIR}${BINDIR}/${DIR}  ; \
        else \
                true ; \
        fi
	( cd ${.CURDIR} ; install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    Makefile ${FILES} ${EXTRA} ${DESTDIR}${BINDIR}/${DIR} )

spell: ${SRCS}
	(cd ${.CURDIR};  spell ${SRCS} ) | sort | \
		comm -23 - ${.CURDIR}/spell.ok > ${DOC}.spell

BINDIR?=	/usr/share/doc
BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	444
