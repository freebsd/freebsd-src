#	$Id: bsd.man.mk,v 1.21 1997/03/08 23:46:55 wosch Exp $
#
# The include file <bsd.man.mk> handles installing manual pages and 
# their links. <bsd.man.mk> includes the file named "../Makefile.inc" 
# if it exists.
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
# MAN${sect}	The manual pages to be installed. For sections see
#		variable ${SECTIONS}
#
# _MANPAGES	List of all man pages to be installed.
#		(``_MANPAGES=$MAN1 $MAN2 ... $MANn'')
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
# MANFILTER	command to pipe the raw man page though before compressing
#		or installing.  Can be used to do sed substitution.
#
# +++ targets +++
#
#	maninstall:
#		Install the manual pages and their links.
#


.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

MANSRC?=	${.CURDIR}
MINSTALL=	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

MCOMPRESS_CMD?=	${COMPRESS_CMD}
MCOMPRESS_EXT?=	${COMPRESS_EXT}

SECTIONS=	1 2 3 4 5 6 7 8 9 n

.undef _MANPAGES
.for sect in ${SECTIONS}
.if defined(MAN${sect}) && !empty(MAN${sect})
.SUFFIXES: .${sect}
.PATH.${sect}: ${MANSRC}
_MANPAGES+= ${MAN${sect}}
.endif
.endfor

# XXX MANDEPEND is only used for groff, man/man, man/manpath, at and atrun.
# It should be named more generally.
all-man: ${MANDEPEND}

.if defined(NOMANCOMPRESS)

COPY=		-c

# Make special arrangements to filter to a temporary file at build time
# for NOMANCOMPRESS.
.if defined(MANFILTER)
FILTEXTENSION=		.filt
.else
FILTEXTENSION=
.endif

ZEXT=

.if defined(MANFILTER)
.for sect in ${SECTIONS}
.if defined(MAN${sect}) && !empty(MAN${sect})
CLEANFILES+=	${MAN${sect}:T:S/$/${FILTEXTENSION}/g}
.for page in ${MAN${sect}}
.for target in ${page:T:S/$/${FILTEXTENSION}/g}
all-man: ${target}
${target}: ${page}
	${MANFILTER} < ${.ALLSRC} > ${.TARGET}
.endfor
.endfor
.endif
.endfor
.endif

.else

ZEXT=		${MCOMPRESS_EXT}

.for sect in ${SECTIONS}
.if defined(MAN${sect}) && !empty(MAN${sect})
CLEANFILES+=	${MAN${sect}:T:S/$/${MCOMPRESS_EXT}/g}
.for page in ${MAN${sect}}
.for target in ${page:T:S/$/${MCOMPRESS_EXT}/}
all-man: ${target}
${target}: ${page}
.if defined(MANFILTER)
	${MANFILTER} < ${.ALLSRC} | ${MCOMPRESS_CMD} > ${.TARGET}
.else
	${MCOMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
.endif
.endfor
.endfor
.endif
.endfor

.endif

maninstall::
.for sect in ${SECTIONS}
.if defined(MAN${sect}) && !empty(MAN${sect})
maninstall:: ${MAN${sect}}
.if defined(NOMANCOMPRESS)
.if defined(MANFILTER)
.for page in ${MAN${sect}}
	${MINSTALL} ${page:T:S/$/${FILTEXTENSION}/g} ${DESTDIR}${MANDIR}${sect}${MANSUBDIR}/${page}
.endfor
.else
	${MINSTALL} ${.ALLSRC} ${DESTDIR}${MANDIR}${sect}${MANSUBDIR}
.endif
.else
	${MINSTALL} ${.ALLSRC:T:S/$/${MCOMPRESS_EXT}/g} \
		${DESTDIR}${MANDIR}${sect}${MANSUBDIR}
.endif
.endif
.endfor

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
.endif
