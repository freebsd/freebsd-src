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
# NO_MLINKS	If you do not want install manual page links. [not set]
#
# MANFILTER	command to pipe the raw man page through before compressing
#		or installing.  Can be used to do sed substitution.
#
# MANBUILDCAT	create preformatted manual pages in addition to normal
#		pages. [not set]
#
# MANDOC_CMD	command and flags to create preformatted pages
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
MANDOC_CMD?=	mandoc -Tascii

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

.if ${MK_MANCOMPRESS} == "no"

# Make special arrangements to filter to a temporary file at build time
# for MK_MANCOMPRESS == no.
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
	${MANFILTER} < ${.ALLSRC} | ${MANDOC_CMD} > ${.TARGET}
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
	${MANDOC_CMD} ${.ALLSRC} > ${.TARGET}
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
.if ${MK_STAGING_MAN} == "yes"
STAGE_TARGETS+= stage_files
_mansets:= ${MAN:E:O:u:M*[1-9]:@s@man$s@}
STAGE_SETS+= ${_mansets}
.for _page in ${MAN}
stage_files.man${_page:T:E}: ${_page}
STAGE_DIR.man${_page:T:E}?= ${STAGE_OBJTOP}${MANDIR}${_page:T:E}${MANSUBDIR}
.endfor
.if !empty(MLINKS)
STAGE_SETS+= mlinks
STAGE_TARGETS+= stage_links
STAGE_LINKS.mlinks:= ${MLINKS:@f@${f:S,^,${MANDIR}${f:E}${MANSUBDIR}/,}@}
stage_links.mlinks: ${_mansets:@s@stage_files.$s@}
.endif
.endif

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
	${MANFILTER} < ${.ALLSRC} | ${MANDOC_CMD} | ${MCOMPRESS_CMD} > ${.TARGET}
.else
	${MANDOC_CMD} ${.ALLSRC} | ${MCOMPRESS_CMD} > ${.TARGET}
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
.if ${MK_MANCOMPRESS} == "no"
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
.for _page _sect in ${.ALLSRC:C/\.([^.]*)$/.\1 \1/}
	@d=${DESTDIR}${MANDIR}${_sect}${MANSUBDIR}/; \
	${ECHO} ${MINSTALL} ${_page} $${d}; \
	${MINSTALL} $${page} $${d};
.endfor
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
		${DESTDIR}${MANDIR}${__page:E}${MANSUBDIR}/
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
	${MINSTALL} ${__page:T:S/$/${CATEXT}${MCOMPRESS_EXT}/g} \
		${DESTDIR}${CATDIR}${__page:E}${MANSUBDIR}/${__page:T:S/$/${MCOMPRESS_EXT}/}
.endif
.endfor
.endif
.endif

.if !defined(NO_MLINKS) && defined(MLINKS) && !empty(MLINKS)
.for _oname _osect _dname _dsect in ${MLINKS:C/\.([^.]*)$/.\1 \1/}
	@l=${DESTDIR}${MANDIR}${_osect}${MANSUBDIR}/${_oname}; \
	t=${DESTDIR}${MANDIR}${_dsect}${MANSUBDIR}/${_dname}; \
	${ECHO} $${t}${ZEXT} -\> $${l}${ZEXT}; \
	rm -f $${t} $${t}${MCOMPRESS_EXT}; \
	${INSTALL_LINK} $${l}${ZEXT} $${t}${ZEXT}
.endfor
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for _oname _osect _dname _dsect in ${MLINKS:C/\.([^.]*)$/.\1 \1/}
	@l=${DESTDIR}${MANDIR}${_osect}${MANSUBDIR}/${_oname}; \
	t=${DESTDIR}${MANDIR}${_dsect}${MANSUBDIR}/${_dname}; \
	${ECHO} $${t}${ZEXT} -\> $${l}${ZEXT}; \
	rm -f $${t} $${t}${MCOMPRESS_EXT}; \
	${INSTALL_LINK} $${l}${ZEXT} $${t}${ZEXT}
.endfor
.endif
.endif

manlint:
.if defined(MAN) && !empty(MAN)
.for __page in ${MAN}
manlint: ${__page}lint
${__page}lint: ${__page}
.if defined(MANFILTER)
	${MANFILTER} < ${.ALLSRC} | ${MANDOC_CMD} -Tlint
.else
	${MANDOC_CMD} -Tlint ${.ALLSRC}
.endif
.endfor
.endif
