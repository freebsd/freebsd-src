# $FreeBSD$
#
# The include file <bsd.man.mk> handles installing manual pages and
# their links.
#
#
# +++ variables +++
#
# DESTDIR	Change the tree where the man pages gets installed. [not set]
#
# MANDIR	Base path for manual installation. [${SHAREDIR}/man/man]
#
# MANOWN	Manual owner. [${SHAREOWN}]
#
# MANGRP	Manual group. [${SHAREGRP}]
#
# MANMODE	Manual mode. [${NOBINMODE}]
#
# MANSUBDIR	Subdirectory under the manual page section, i.e. "/i386"
#		or "/tahoe" for machine specific manual pages.
#
# MAN		The manual pages to be installed. For sections see
#		variable ${SECTIONS}
#
# MCOMPRESS_CMD	Program to compress man pages. Output is to
#		stdout. [${COMPRESS_CMD}]
#
# MLINKS	List of manual page links (using a suffix). The
#		linked-to file must come first, the linked file
#		second, and there may be multiple pairs. The files
#		are hard-linked.
#
# NO_MANCOMPRESS	If you do not want unformatted manual pages to be
#		compressed when they are installed. [not set]
#
# NO_MLINKS	If you do not want install manual page links. [not set]
#
# MANFILTER	command to pipe the raw man page through before compressing
#		or installing.  Can be used to do sed substitution.
#
# MANBUILDCAT	create preformatted manual pages in addition to normal
#		pages. [not set]
#
# MROFF_CMD	command and flags to create preformatted pages
#
# +++ targets +++
#
#	maninstall:
#		Install the manual pages and their links.
#

.if !target(__<bsd.init.mk>__)
.error bsd.man.mk cannot be included directly.
.endif

MINSTALL?=	${INSTALL} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

CATDIR=		${MANDIR:H:S/$/\/cat/}
CATEXT=		.cat
MROFF_CMD?=	groff -Tascii -mtty-char -man -t

MCOMPRESS_CMD?=	${COMPRESS_CMD}
MCOMPRESS_EXT?=	${COMPRESS_EXT}

SECTIONS=	1 2 3 4 5 6 7 8 9
.SUFFIXES:	${SECTIONS:S/^/./g}

# Backwards compatibility.
.if !defined(MAN)
.for __sect in ${SECTIONS}
.if defined(MAN${__sect}) && !empty(MAN${__sect})
MAN+=	${MAN${__sect}}
.endif
.endfor
.endif

_manpages:
all-man: _manpages

.if defined(NO_MANCOMPRESS)

# Make special arrangements to filter to a temporary file at build time
# for NO_MANCOMPRESS.
.if defined(MANFILTER)
FILTEXTENSION=		.filt
.else
FILTEXTENSION=
.endif

ZEXT=

.if defined(MANFILTER)
.if defined(MAN) && !empty(MAN)
CLEANFILES+=	${MAN:T:S/$/${FILTEXTENSION}/g}
CLEANFILES+=	${MAN:T:S/$/${CATEXT}${FILTEXTENSION}/g}
.for __page in ${MAN}
.for __target in ${__page:T:S/$/${FILTEXTENSION}/g}
_manpages: ${__target}
${__target}: ${__page}
	${MANFILTER} < ${.ALLSRC} > ${.TARGET}
.endfor
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for __target in ${__page:T:S/$/${CATEXT}${FILTEXTENSION}/g}
_manpages: ${__target}
${__target}: ${__page}
	${MANFILTER} < ${.ALLSRC} | ${MROFF_CMD} > ${.TARGET}
.endfor
.endif
.endfor
.endif
.else
.if defined(MAN) && !empty(MAN)
CLEANFILES+=	${MAN:T:S/$/${CATEXT}/g}
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for __page in ${MAN}
.for __target in ${__page:T:S/$/${CATEXT}/g}
_manpages: ${__target}
${__target}: ${__page}
	${MROFF_CMD} ${.ALLSRC} > ${.TARGET}
.endfor
.endfor
.else
_manpages: ${MAN}
.endif
.endif
.endif

.else

ZEXT=		${MCOMPRESS_EXT}

.if defined(MAN) && !empty(MAN)
CLEANFILES+=	${MAN:T:S/$/${MCOMPRESS_EXT}/g}
CLEANFILES+=	${MAN:T:S/$/${CATEXT}${MCOMPRESS_EXT}/g}
.for __page in ${MAN}
.for __target in ${__page:T:S/$/${MCOMPRESS_EXT}/}
_manpages: ${__target}
${__target}: ${__page}
.if defined(MANFILTER)
	${MANFILTER} < ${.ALLSRC} | ${MCOMPRESS_CMD} > ${.TARGET}
.else
	${MCOMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
.endif
.endfor
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for __target in ${__page:T:S/$/${CATEXT}${MCOMPRESS_EXT}/}
_manpages: ${__target}
${__target}: ${__page}
.if defined(MANFILTER)
	${MANFILTER} < ${.ALLSRC} | ${MROFF_CMD} | ${MCOMPRESS_CMD} > ${.TARGET}
.else
	${MROFF_CMD} ${.ALLSRC} | ${MCOMPRESS_CMD} > ${.TARGET}
.endif
.endfor
.endif
.endfor
.endif

.endif

maninstall: _maninstall
_maninstall:
.if defined(MAN) && !empty(MAN)
_maninstall: ${MAN}
.if defined(NO_MANCOMPRESS)
.if defined(MANFILTER)
.for __page in ${MAN}
	${MINSTALL} ${__page:T:S/$/${FILTEXTENSION}/g} \
		${DESTDIR}${MANDIR}${__page:E}${MANSUBDIR}/${__page}
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
	${MINSTALL} ${__page:T:S/$/${CATEXT}${FILTEXTENSION}/g} \
		${DESTDIR}${CATDIR}${__page:E}${MANSUBDIR}/${__page}
.endif
.endfor
.else
	@set ${.ALLSRC:C/\.([^.]*)$/.\1 \1/}; \
	while : ; do \
		case $$# in \
			0) break;; \
			1) echo "warn: missing extension: $$1"; break;; \
		esac; \
		page=$$1; shift; sect=$$1; shift; \
		d=${DESTDIR}${MANDIR}$${sect}${MANSUBDIR}; \
		${ECHO} ${MINSTALL} $${page} $${d}; \
		${MINSTALL} $${page} $${d}; \
	done
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for __page in ${MAN}
	${MINSTALL} ${__page:T:S/$/${CATEXT}/} \
		${DESTDIR}${CATDIR}${__page:E}${MANSUBDIR}/${__page:T}
.endfor
.endif
.endif
.else
.for __page in ${MAN}
	${MINSTALL} ${__page:T:S/$/${MCOMPRESS_EXT}/g} \
		${DESTDIR}${MANDIR}${__page:E}${MANSUBDIR}
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
	${MINSTALL} ${__page:T:S/$/${CATEXT}${MCOMPRESS_EXT}/g} \
		${DESTDIR}${CATDIR}${__page:E}${MANSUBDIR}/${__page:T:S/$/${MCOMPRESS_EXT}/}
.endif
.endfor
.endif
.endif

.if !defined(NO_MLINKS) && defined(MLINKS) && !empty(MLINKS)
	@set ${MLINKS:C/\.([^.]*)$/.\1 \1/}; \
	while : ; do \
		case $$# in \
			0) break;; \
			[123]) echo "warn: empty MLINK: $$1 $$2 $$3"; break;; \
		esac; \
		name=$$1; shift; sect=$$1; shift; \
		l=${DESTDIR}${MANDIR}$${sect}${MANSUBDIR}/$$name; \
		name=$$1; shift; sect=$$1; shift; \
		t=${DESTDIR}${MANDIR}$${sect}${MANSUBDIR}/$$name; \
		${ECHO} $${t}${ZEXT} -\> $${l}${ZEXT}; \
		rm -f $${t} $${t}${MCOMPRESS_EXT}; \
		${INSTALL_LINK} $${l}${ZEXT} $${t}${ZEXT}; \
	done
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
	@set ${MLINKS:C/\.([^.]*)$/.\1 \1/}; \
	while : ; do \
		case $$# in \
			0) break;; \
			[123]) echo "warn: empty MLINK: $$1 $$2 $$3"; break;; \
		esac; \
		name=$$1; shift; sect=$$1; shift; \
		l=${DESTDIR}${CATDIR}$${sect}${MANSUBDIR}/$$name; \
		name=$$1; shift; sect=$$1; shift; \
		t=${DESTDIR}${CATDIR}$${sect}${MANSUBDIR}/$$name; \
		${ECHO} $${t}${ZEXT} -\> $${l}${ZEXT}; \
		rm -f $${t} $${t}${MCOMPRESS_EXT}; \
		${INSTALL_LINK} $${l}${ZEXT} $${t}${ZEXT}; \
	done
.endif
.endif

manlint:
.if defined(MAN) && !empty(MAN)
.for __page in ${MAN}
manlint: ${__page}lint
${__page}lint: ${__page}
.if defined(MANFILTER)
	${MANFILTER} < ${.ALLSRC} | ${MROFF_CMD} -ww -z
.else
	${MROFF_CMD} -ww -z ${.ALLSRC}
.endif
.endfor
.endif
