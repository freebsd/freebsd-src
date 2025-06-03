# SPDX-License-Identifier: BSD-2-Clause
#
#	$Id: subdir.mk,v 1.27 2024/09/01 05:02:43 sjg Exp $
#
#	@(#) Copyright (c) 2002-2024, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net

#	if SUBDIR=@auto replace that with each subdir that has
#	a [Mm]akefile.
#
#	Unless SUBDIR_MUST_EXIST is defined, missing subdirs
#	are ignored (to allow for sparse checkout).
#
#	If you use _SUBDIRUSE for a target you may need to add it to
#	SUBDIR_TARGETS.

# should be set properly in sys.mk
_this ?= ${.PARSEFILE:S,bsd.,,}

.if !target(__${_this}__)
__${_this}__: .NOTMAIN

.if defined(SUBDIR) || defined(SUBDIR.yes)

.if ${.MAKE.LEVEL} == 0 && ${MK_DIRDEPS_BUILD:Uno} == "yes"
.include <meta.subdir.mk>
# keep everyone happy
_SUBDIRUSE:
.elif !commands(_SUBDIRUSE) && !defined(NO_SUBDIR) && !defined(NOSUBDIR)
.-include <local.subdir.mk>
.if !target(.MAIN)
.MAIN: all
.endif

ECHO_DIR ?= echo
.ifdef SUBDIR_MUST_EXIST
MISSING_DIR=echo "Missing ===> ${.CURDIR}/$$_dir"; exit 1
.else
MISSING_DIR=echo "Skipping ===> ${.CURDIR}/$$_dir"; exit 0
.endif

# the actual implementation
# our target should be of the form ${_target}-${_dir}
_SUBDIR_USE: .USE
	@Exists() { test -f $$1; }; \
	_dir=${.TARGET:C/^[^-]*-//} \
	_target=${.TARGET:C/-.*//:S/real//:S/.depend/depend/}; \
	if ! Exists ${.CURDIR}/$$_dir/[mM]akefile; then \
		${MISSING_DIR}; \
	fi; \
	if test X"${_THISDIR_}" = X""; then \
		_nextdir_="$$_dir"; \
	else \
		_nextdir_="$${_THISDIR_}/$$_dir"; \
	fi; \
	${ECHO_DIR} "===> $${_nextdir_} ($$_target)"; \
	(cd ${.CURDIR}/$$_dir && \
		${.MAKE} _THISDIR_="$${_nextdir_}" $$_target)

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif
install: maninstall
maninstall: afterinstall
afterinstall: realinstall
realinstall: beforeinstall _SUBDIRUSE
.endif

# the interface from others
# this may require additions to SUBDIR_TAREGTS
_SUBDIRUSE: .USE subdir-${.TARGET:C/-.*//:S/real//:S/.depend/depend/}

SUBDIR_TARGETS += \
	all \
	clean \
	cleandir \
	includes \
	install \
	depend \
	lint \
	obj \
	tags \
	etags

.if ${SUBDIR:U} == "@auto"
SUBDIR = ${echo ${.CURDIR}/*/[Mm]akefile:L:sh:H:T:O:N\*}
.endif
# allow for things like SUBDIR.${MK_TESTS}
SUBDIR += ${SUBDIR.yes:U}

__subdirs =
.for d in ${SUBDIR}
.if $d != ".WAIT" && exists(${.CURDIR}/$d.${MACHINE})
__subdirs += $d.${MACHINE}
.else
__subdirs += $d
.endif
.endfor

.for t in ${SUBDIR_TARGETS:O:u}
__subdir_$t =
.for d in ${__subdirs}
.if $d == ".WAIT"
__subdir_$t += $d
.elif !commands($t-$d)
$t-$d: .PHONY .MAKE _SUBDIR_USE
__subdir_$t += $t-$d
.endif
.endfor
subdir-$t: .PHONY ${__subdir_$t}
$t: subdir-$t
.endfor

.else
_SUBDIRUSE:
.endif				# SUBDIR

.include <own.mk>
.if make(destroy*)
.include <obj.mk>
.endif
.endif
# make sure this exists
all:

.endif
