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
# NOMANCOMPRESS	If you do not want unformatted manual pages to be 
#		compressed when they are installed. [not set]
#
# NOMLINKS	If you do not want install manual page links. [not set]
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

MINSTALL=	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

CATDIR=		${MANDIR:H:S/$/\/cat/}
CATEXT=		.cat
MROFF_CMD?=	groff -Tascii -mtty-char -man -t

MCOMPRESS_CMD?=	${COMPRESS_CMD}
MCOMPRESS_EXT?=	${COMPRESS_EXT}

SECTIONS=	1 1aout 2 3 4 5 6 7 8 9
.SUFFIXES:	${SECTIONS:S/^/./g}

# Backwards compatibility.
.if !defined(MAN)
.for sect in ${SECTIONS}
.if defined(MAN${sect}) && !empty(MAN${sect})
MAN+=	${MAN${sect}}
.endif
.endfor
.endif

all-man:

.if defined(NOMANCOMPRESS)

# Make special arrangements to filter to a temporary file at build time
# for NOMANCOMPRESS.
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
.for page in ${MAN}
.for target in ${page:T:S/$/${FILTEXTENSION}/g}
all-man: ${target}
${target}: ${page}
	${MANFILTER} < ${.ALLSRC} > ${.TARGET}
.endfor
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for target in ${page:T:S/$/${CATEXT}${FILTEXTENSION}/g}
all-man: ${target}
${target}: ${page}
	${MANFILTER} < ${.ALLSRC} | ${MROFF_CMD} > ${.TARGET}
.endfor
.endif
.endfor
.endif
.else
.if defined(MAN) && !empty(MAN)
CLEANFILES+=	${MAN:T:S/$/${CATEXT}/g}
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for page in ${MAN}
.for target in ${page:T:S/$/${CATEXT}/g}
all-man: ${target}
${target}: ${page}
	${MROFF_CMD} ${.ALLSRC} > ${.TARGET}
.endfor
.endfor
.endif
.endif
.endif

.else

ZEXT=		${MCOMPRESS_EXT}

.if defined(MAN) && !empty(MAN)
CLEANFILES+=	${MAN:T:S/$/${MCOMPRESS_EXT}/g}
CLEANFILES+=	${MAN:T:S/$/${CATEXT}${MCOMPRESS_EXT}/g}
.for page in ${MAN}
.for target in ${page:T:S/$/${MCOMPRESS_EXT}/}
all-man: ${target}
${target}: ${page}
.if defined(MANFILTER)
	${MANFILTER} < ${.ALLSRC} | ${MCOMPRESS_CMD} > ${.TARGET}
.else
	${MCOMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
.endif
.endfor
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for target in ${page:T:S/$/${CATEXT}${MCOMPRESS_EXT}/}
all-man: ${target}
${target}: ${page}
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

maninstall::
.if defined(MAN) && !empty(MAN)
maninstall:: ${MAN}
.if defined(NOMANCOMPRESS)
.if defined(MANFILTER)
.for page in ${MAN}
	${MINSTALL} ${page:T:S/$/${FILTEXTENSION}/g} \
		${DESTDIR}${MANDIR}${page:E}${MANSUBDIR}/${page}
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
	${MINSTALL} ${page:T:S/$/${CATEXT}${FILTEXTENSION}/g} \
		${DESTDIR}${CATDIR}${page:E}${MANSUBDIR}/${page}
.endif
.endfor
.else
	@set `echo ${.ALLSRC} " " | sed 's/\.\([^.]*\) /.\1 \1 /g'`; \
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
.for page in ${MAN}
	${MINSTALL} ${page:T:S/$/${CATEXT}/} \
		${DESTDIR}${CATDIR}${page:E}${MANSUBDIR}/${page:T}
.endfor
.endif
.endif
.else
.for page in ${MAN}
	${MINSTALL} ${page:T:S/$/${MCOMPRESS_EXT}/g} \
		${DESTDIR}${MANDIR}${page:E}${MANSUBDIR}
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
	${MINSTALL} ${page:T:S/$/${CATEXT}${MCOMPRESS_EXT}/g} \
		${DESTDIR}${CATDIR}${page:E}${MANSUBDIR}/${page:T:S/$/${MCOMPRESS_EXT}/}
.endif
.endfor
.endif
.endif

.if !defined(NOMLINKS) && defined(MLINKS) && !empty(MLINKS)
	@set `echo ${MLINKS} " " | sed 's/\.\([^.]*\) /.\1 \1 /g'`; \
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
		ln $${l}${ZEXT} $${t}${ZEXT}; \
	done
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
	@set `echo ${MLINKS} " " | sed 's/\.\([^.]*\) /.\1 \1 /g'`; \
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
		ln $${l}${ZEXT} $${t}${ZEXT}; \
	done
.endif
.endif
