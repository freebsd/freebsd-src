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
# MANSRC.${MAN:T} Name of source file for an individual manual page.
#		Defaults to the manual page name.
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
# MANGROUPS	A list of groups, each of which should be a variable containing
# 		a list of manual pages in that group.  By default one group is
# 		defined called "MAN".
#
# 		For each group, group-specific options may be set:
# 		<group>OWN, <group>GRP, <group>MODE and <group>PACKAGE.
#
# +++ targets +++
#
#	maninstall:
#		Install the manual pages and their links.
#

.if !target(__<bsd.init.mk>__)
.error bsd.man.mk cannot be included directly.
.endif

MANGROUPS?=	MAN

# MAN_SUBPACKAGE is the subpackage manpages will be installed in.  When
# MANSPLITPKG is enabled, this is ignored and the subpackage is forced
# to be "-man", otherwise it defaults to empty so manpages go in the
# base package.  This can be set to "-dev" for manpages that should go
# in the -dev package.
MAN_SUBPACKAGE?=

# The default man package, if not otherwise specified.
MAN_PACKAGE=	${PACKAGE:Uutilities}

# Backwards compatibility.
MINSTALL?=	${MANINSTALL}

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
MANGROUPS+=	MAN${__sect}
.endfor
.endif

# Following the conventions of MANGROUPS, manpage links should be defined
# as ${group}LINKS, which means the default groups' links would be called
# MANLINKS.  However it's actually called MLINKS, so for compatibility,
# use ${MLINKS} as the default group's links if it's set.
.if defined(MLINKS)
MANLINKS=	${MLINKS}
.endif

maninstall: realmaninstall manlinksinstall .PHONY
# Make sure all manpages are installed before we try to link any.
.ORDER: realmaninstall manlinksinstall
realmaninstall: .PHONY
manlinksinstall: .PHONY

all-man:

# Take groups from both MANGROUPS and MANGROUPS.yes, to allow syntax like
# MANGROUPS.${MK_FOO}=FOO.  Sort and uniq the list of groups in case of
# duplicates.
.if defined(MANGROUPS) || defined(MANGROUPS.yes)
MANGROUPS:=${MANGROUPS} ${MANGROUPS.yes}
MANGROUPS:=${MANGROUPS:O:u}
.endif

.for __group in ${MANGROUPS}

realmaninstall: realmaninstall-${__group}
manlinksinstall: manlinksinstall-${__group}

${__group}OWN?=		${MANOWN}
${__group}GRP?=		${MANGRP}
${__group}MODE?=	${MANMODE}
# If MANSPLITPKG is enabled, ignore the requested man subpackage and put the
# manpages in -man instead.
.if ${MK_MANSPLITPKG} == "yes"
${__group}SUBPACKAGE=	-man
.else
${__group}SUBPACKAGE?=	${MAN_SUBPACKAGE}
.endif
${__group}PACKAGE?=	${MAN_PACKAGE}${${__group}SUBPACKAGE}

# Tag processing is only done for NO_ROOT installs.
.if defined(NO_ROOT)
.if !defined(${__group}TAGS) || ! ${${__group}TAGS:Mpackage=*}
${__group}TAGS+=       package=${${__group}PACKAGE}
.endif

${__group}TAG_ARGS=	-T ${${__group}TAGS:ts,:[*]}
.endif	# defined(NO_ROOT)

${__group}INSTALL?=	${INSTALL} ${${__group}TAG_ARGS} \
	-o ${${__group}OWN} -g ${${__group}GRP} -m ${${__group}MODE}

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
.if defined(${__group}) && !empty(${__group})
CLEANFILES+=	${${__group}:T:S/$/${FILTEXTENSION}/g}
CLEANFILES+=	${${__group}:T:S/$/${CATEXT}${FILTEXTENSION}/g}
.for __page in ${${__group}}
# Escape colons in target names to support manual pages whose
# filenames contain colons.
.for __target in ${__page:T:S/:/\:/g:S/$/${FILTEXTENSION}/g}
all-man: ${__target}
${__target}: ${MANSRC.${__page:T}:U${__page}}
	${MANFILTER} < ${.ALLSRC} > ${.TARGET}
.endfor
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for __target in ${__page:T:S/:/\:/g:S/$/${CATEXT}${FILTEXTENSION}/g}
all-man: ${__target}
${__target}: ${MANSRC.${__page:T}:U${__page}}
	${MANFILTER} < ${.ALLSRC} | ${MANDOC_CMD} > ${.TARGET}
.endfor
.endif
.endfor
.endif	# !empty(${__group})
.else	# !defined(MANFILTER)
.if defined(${__group}) && !empty(${__group})
CLEANFILES+=	${${__group}:T:S/$/${CATEXT}/g}
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for __page in ${${__group}}
.for __target in ${__page:T:S/:/\:/g:S/$/${CATEXT}/g}
all-man: ${__target}
${__target}: ${MANSRC.${__page:T}:U${__page}}
	${MANDOC_CMD} ${.ALLSRC} > ${.TARGET}
.endfor
.endfor
.else
.for __page in ${${__group}}
.if defined(MANSRC.${__page:T})
.for __target in ${__page:T:S/:/\:/g}
all-man: ${__target}
CLEANFILES+=	${__target}
${__target}: ${MANSRC.${__page:T}}
	${CP} ${.ALLSRC} ${.TARGET}
.endfor
.else
all-man: ${__page}
.endif
.endfor
.endif
.endif
.endif	# defined(MANFILTER)

.else	# ${MK_MANCOMPRESS} == "yes"

ZEXT=		${MCOMPRESS_EXT}

.if defined(${__group}) && !empty(${__group})
CLEANFILES+=	${${__group}:T:S/$/${MCOMPRESS_EXT}/g}
CLEANFILES+=	${${__group}:T:S/$/${CATEXT}${MCOMPRESS_EXT}/g}
.for __page in ${${__group}}
.for __target in ${__page:T:S/:/\:/g:S/$/${MCOMPRESS_EXT}/}
all-man: ${__target}
${__target}: ${MANSRC.${__page:T}:U${__page}}
.if defined(MANFILTER)
	${MANFILTER} < ${.ALLSRC} | ${MCOMPRESS_CMD} > ${.TARGET}
.else
	${MCOMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
.endif
.endfor
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for __target in ${__page:T:S/:/\:/g:S/$/${CATEXT}${MCOMPRESS_EXT}/}
all-man: ${__target}
${__target}: ${MANSRC.${__page:T}:U${__page}}
.if defined(MANFILTER)
	${MANFILTER} < ${.ALLSRC} | ${MANDOC_CMD} | ${MCOMPRESS_CMD} > ${.TARGET}
.else
	${MANDOC_CMD} ${.ALLSRC} | ${MCOMPRESS_CMD} > ${.TARGET}
.endif
.endfor
.endif
.endfor
.endif

.endif	# ${MK_MANCOMPRESS} == "no"

_MANLINKS=
.if !defined(NO_MLINKS) && defined(${__group}LINKS) && !empty(${__group}LINKS)
.for _oname _osect _dname _dsect in ${${__group}LINKS:C/\.([^.]*)$/.\1 \1/}
_MANLINKS+=	${MANDIR}${_osect}${MANSUBDIR}/${_oname} \
		${MANDIR}${_dsect}${MANSUBDIR}/${_dname}
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
_MANLINKS+=	${CATDIR}${_osect}${MANSUBDIR}/${_oname} \
		${CATDIR}${_dsect}${MANSUBDIR}/${_dname}
.endif
.endfor
.endif

.if defined(${__group}) && !empty(${__group})
.if ${MK_STAGING_MAN} == "yes"
STAGE_TARGETS+= stage_files.${__group}
_mansets.${__group}:= ${${__group}:E:O:u:M*[1-9]:@s@man$s@}
STAGE_SETS+= ${_mansets.${__group}}
.for _page in ${${__group}}
stage_files.${__group}.man${_page:T:E}: ${_page}
.if target(${_page}${MCOMPRESS_EXT})
stage_files.${__group}.man${_page:T:E}: ${_page}${MCOMPRESS_EXT}
.endif
STAGE_DIR.${__group}.man${_page:T:E}?= ${STAGE_OBJTOP}${MANDIR}${_page:T:E}${MANSUBDIR}
.endfor
.if !defined(NO_MLINKS) && !empty(${__group}LINKS)
STAGE_SETS+= mlinks.${__group}
STAGE_TARGETS+= stage_links.${__group}
STAGE_LINKS.mlinks.${__group}:= ${${__group}LINKS:M*.[1-9]:@f@${f:S,^,${MANDIR}${f:E}${MANSUBDIR}/,}@}
stage_links.mlinks.${__group}: ${_mansets.${__group}:@s@stage_files.${__group}.$s@}
.endif
.endif
.endif

realmaninstall-${__group}:
.if defined(${__group}) && !empty(${__group})
.for __page in ${${__group}}
__mansrc.${__group}+=	${MANSRC.${__page:T}:U${__page}}
.endfor
realmaninstall-${__group}: ${__mansrc.${__group}}
.if ${MK_MANCOMPRESS} == "no"
.if defined(MANFILTER)
.for __page in ${${__group}}
	${${__group}INSTALL} ${__page:T:S/$/${FILTEXTENSION}/g} \
		${DESTDIR}${MANDIR}${__page:E}${MANSUBDIR}/${__page}
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
	${${__group}INSTALL} ${__page:T:S/$/${CATEXT}${FILTEXTENSION}/g} \
		${DESTDIR}${CATDIR}${__page:E}${MANSUBDIR}/${__page}
.endif
.endfor
.else	# !defined(MANFILTER)
	@set ${.ALLSRC:C/\.([^.]*)$/.\1 \1/}; \
	while : ; do \
		case $$# in \
			0) break;; \
			1) echo "warn: missing extension: $$1"; break;; \
		esac; \
		page=$$1; shift; sect=$$1; shift; \
		d=${DESTDIR}${MANDIR}$${sect}${MANSUBDIR}; \
		${ECHO} ${${__group}INSTALL} $${page} $${d}; \
		${${__group}INSTALL} $${page} $${d}; \
	done
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
.for __page in ${${__group}}
	${${__group}INSTALL} ${__page:T:S/$/${CATEXT}/} \
		${DESTDIR}${CATDIR}${__page:E}${MANSUBDIR}/${__page:T}
.endfor
.endif
.endif	# defined(MANFILTER)
.else	# ${MK_MANCOMPRESS} == "yes"
.for __page in ${${__group}}
	${${__group}INSTALL} ${__page:T:S/$/${MCOMPRESS_EXT}/g} \
		${DESTDIR}${MANDIR}${__page:E}${MANSUBDIR}/
.if defined(MANBUILDCAT) && !empty(MANBUILDCAT)
	${${__group}INSTALL} ${__page:T:S/$/${CATEXT}${MCOMPRESS_EXT}/g} \
		${DESTDIR}${CATDIR}${__page:E}${MANSUBDIR}/${__page:T:S/$/${MCOMPRESS_EXT}/}
.endif
.endfor
.endif	# ${MK_MANCOMPRESS} == "no"
.endif

manlinksinstall-${__group}:
.for l t in ${_MANLINKS}
# On MacOS, assume case folding FS, and don't install links from foo.x to FOO.x.
.if ${.MAKE.OS} != "Darwin" || ${l:tu} != ${t:tu}
	${INSTALL_MANLINK} ${${__group}TAG_ARGS} ${DESTDIR}${l}${ZEXT} ${DESTDIR}${t}${ZEXT}
.endif
.endfor

manlint: .PHONY checkmanlinks
.if defined(${__group}) && !empty(${__group})
.for __page in ${${__group}}
manlint: ${__page:S/:/\:/g}lint
${__page:S/:/\:/g}lint: .PHONY ${MANSRC.${__page:T}:U${__page}}
.if defined(MANFILTER)
	${MANFILTER} < ${.ALLSRC} | ${MANDOC_CMD} -Tlint
.else
	${MANDOC_CMD} -Tlint ${.ALLSRC}
.endif
.endfor
.endif

checkmanlinks: .PHONY
.if defined(${__group}LINKS)
checkmanlinks: checkmanlinks-${__group}
checkmanlinks-${__group}: .PHONY
.for __page __link in ${${__group}LINKS}
checkmanlinks-${__group}: checkmanlinks-${__group}-${__link}
checkmanlinks-${__group}-${__link}: .PHONY ${__page}
	@if ! egrep -q "^(\.\\\\\" )?\.Nm ${__link:R}( ,)?$$" ${.ALLSRC}; then \
		echo "${__group}LINKS: '.Nm ${__link:R}' not found in ${__page}"; \
		exit 1; \
	fi >&2
.endfor # __page __link in ${${__group}LINKS}
.endif # defined(${__group}LINKS)

.endfor	# __group in ${MANGROUPS}
