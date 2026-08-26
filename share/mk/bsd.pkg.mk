# SPDX-License-Identifier: ISC
#
# Copyright (c) 2026 Lexi Winter <ivy@FreeBSD.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# bsd.pkg.mk: Create a package based on an existing plist and the template
# UCL files.  This is intended for use by packages/* during the build, and
# cannot be used to generate standalone packages.

.if !defined(REPODIR)
. error REPODIR must be set
.endif

.if !defined(PKG_VERSION)
. error PKG_VERSION must be set
.endif

_PKG_NEED_ABI=
.include <bsd.pkg.pre.mk>
.include <bsd.compat.pre.mk>
.include <src.opts.mk>

# Allow flua to be overridden; the world build does this to use the
# bootstrap version.
FLUA?=	/usr/libexec/flua

# The directory that files to be packaged have been staged into.
# If ${WSTAGEDIR} is set from Makefile.inc1, use that, otherwise
# use the default location.
.if defined(WSTAGEDIR)
_PKG_WORLDSTAGE=	${WSTAGEDIR}
.else
_PKG_WORLDSTAGE=	${OBJTOP}/worldstage
.endif

_PKGDIR=	${SRCTOP}/packages
_STAGEDIR=	${REPODIR}/${PKG_ABI}/${PKG_VERSION}

# These are the default UCL variables we pass to generate-ucl.lua.
# Allow the caller to add additional variables via PKG_UCLVARS.
_UCLVARS= \
	VERSION "${PKG_VERSION}" \
	PKG_NAME_PREFIX "${PKG_NAME_PREFIX}" \
	PKG_WWW "${PKG_WWW}" \
	PKG_MAINTAINER "${PKG_MAINTAINER}" \
	${PKG_UCLVARS}

# Global pkg(8) arguments.
_PKG_ARGS= \
	-o ABI=${PKG_ABI} \
	-o OSVERSION=${PKG_OSVERSION} \
	-o ALLOW_BASE_SHLIBS=yes \
	-o SHLIB_PROVIDE_PATHS_NATIVE=/lib,/usr/lib \
	${_ALL_LIBCOMPATS:range:@i@-o SHLIB_PROVIDE_PATHS_COMPAT_${_ALL_LIBCOMPATS:[$i]}=/usr/lib${_ALL_libcompats:[$i]}@} \

# Arguments to pkg-create(8).
_PKG_CREATE_ARGS= \
	-f ${PKG_FORMAT} \
	${PKG_CLEVEL} \
	-T ${PKG_CTHREADS} \
	-r ${_PKG_WORLDSTAGE} \
	-o ${.OBJDIR}

_PKG_CREATE=		${PKG_CMD} ${_PKG_ARGS} create ${_PKG_CREATE_ARGS}

# If WORLDPACKAGE is set, we're building a world package.
.if defined(WORLDPACKAGE)

# Default to using the same UCL file for all subpackages, since the vast
# majority of packages are built this way.
UCLSRC?=	${WORLDPACKAGE}.ucl

# Nearly all packages have a dbg and man subpackage, so enable that by default.
SUBPACKAGES?=	dbg man

# This lets the caller use constructs like SUBPACKAGES.${MK_FOO}+= foo
_SUBPACKAGES=	${SUBPACKAGES} ${SUBPACKAGES.yes} ${SUBPACKAGES.yes.yes}

# Remove the dbg subpackage if debug files are disabled.
.if ${MK_DEBUG_FILES} == "no"
_SUBPACKAGES:=	${_SUBPACKAGES:Ndbg}
. if defined(COMPAT_PKGS)
COMPAT_PKGS:=	${COMPAT_PKGS:Ndbg}
. endif	# defined(COMPAT_PKGS)
.endif	# ${MK_DEBUG_FILES} == "no"

# Remove the man subpackage if split man packages are disabled.
.if ${MK_MANSPLITPKG} == "no"
_SUBPACKAGES:=	${_SUBPACKAGES:Nman}
.endif	# ${MK_MANSPLITPKG} == "no"

# Add the base package, unless there isn't one, which can happen with
# static library packages.
.if !defined(PKG_NO_BASE)
_ALLPACKAGES=	${WORLDPACKAGE}
.endif

# Add basic subpackages.
.for _subpackage in ${_SUBPACKAGES}
_ALLPACKAGES+=	${_subpackage:S/^/${WORLDPACKAGE}-/}
.endfor	# _subpackage in ${_SUBPACKAGES}

# Add libcompat subpackages.
.for LIBCOMPAT libcompat in ${_ALL_LIBCOMPATS_libcompats}
. if ${MK_LIB${LIBCOMPAT}} != "no"

# For the base package...
.  if ${COMPAT_PKG:Uno} != "no"
_ALLPACKAGES+=	${WORLDPACKAGE}-lib${libcompat}
.  endif

# And for subpackages.
.  for _pkg in ${COMPAT_PKGS}
# lib is special, because it gets -lib32, not -lib-lib32.
.   if ${_pkg} == "lib"
_ALLPACKAGES+=	${WORLDPACKAGE}-lib${libcompat}
.   else
_ALLPACKAGES+=	${WORLDPACKAGE}-${_pkg}-lib${libcompat}
.   endif
.  endfor

. endif	# ${MK_LIB${LIBCOMPAT}} != "no"
.endfor

CLEANFILES+=	*.pkgucl *.ucl.in *.plist *.pkg

all: .PHONY

. for _pkg in ${_ALLPACKAGES}
_pkgfullname:=	${PKG_NAME_PREFIX}-${_pkg}
_pkgfilename:=	${_pkgfullname}-${PKG_VERSION}.pkg

all: ${_pkgfilename}

${_pkgfilename}: ${_pkgfullname}.pkgucl ${_pkg}.plist
	# We should never create an empty package; this means we intended to
	# build the package, but we didn't build the things which are supposed
	# to be in the package, which means something went wrong somewhere.
	# If you hit this check when building, it probably means a package is
	# not properly excluded in packages/Makefile based on src.conf options.
	@if [ "$$(grep -vc '^@dir' "${.ALLSRC:M*.plist}")" -eq 0 ]; \
	then \
		printf >&2 'ERROR: Refusing to build empty package %s from %s\n' \
			"${.TARGET}" "${.ALLSRC:M*.plist}"; \
		exit 1; \
	fi
	${_PKG_CREATE} -M ${.ALLSRC:M*.pkgucl} -p ${.ALLSRC:M*.plist}

${_pkgfullname}.ucl.in: ${UCLSRC.${_pkg}:U${UCLSRC:U${_pkg}.ucl}}
	@echo 'Generating ${.TARGET} from ${.ALLSRC}'
	@cp ${.ALLSRC} ${.TARGET}
	@echo >>${.TARGET} 'name = "${PKG_NAME_PREFIX}-${_pkg}"'
	@echo >>${.TARGET} 'origin = "base/${PKG_NAME_PREFIX}-${_pkg}"'
	@echo >>${.TARGET} 'categories [ "base" ]'
	@echo >>${.TARGET} 'prefix = "/"'
	@echo >>${.TARGET} 'version = "${PKG_VERSION}"'
	@echo >>${.TARGET} 'maintainer = "${PKG_MAINTAINER}"'
	@echo >>${.TARGET} 'www = "${PKG_WWW}"'
.if defined(PKG_VITAL.${_pkg})
	@echo >>${.TARGET} 'vital = true'
.endif
	@echo >>${.TARGET} 'licenselogic = "${PKG_LICENSELOGIC}"'
	@echo >>${.TARGET} 'licenses ['
.for _license in ${PKG_LICENSES}
	# When a value contains spaces and is quoted (e.g., "X WITH Y"),
	# make preserves the quotes in the value, so we need to strip them.
	@echo >>${.TARGET} '    "${_license:S/"//g}",'
.endfor
	@echo >>${.TARGET} ']'
	@echo >>${.TARGET} 'annotations {'
.for _annotation in ${PKG_ANNOTATIONS}
	@echo >>${.TARGET} '    ${_annotation} = "${PKG_ANNOTATIONS.${_annotation}}"'
.endfor
	@echo >>${.TARGET} '}'
.if defined(PKG_DEPS.${_pkg})
	@echo >>${.TARGET} 'deps {'
. for _dep in ${PKG_DEPS.${_pkg}}
	@echo >>${.TARGET} '    "${_dep}" {'
	@echo >>${.TARGET} '        "origin" = "base/${PKG_NAME_PREFIX}-${_dep}"'
	@echo >>${.TARGET} '        "version" = "${PKG_VERSION}"'
	@echo >>${.TARGET} '    },'
. endfor
	@echo >>${.TARGET} '}'
.endif

${_pkgfullname}.pkgucl: ${_pkgfullname}.ucl.in
	${FLUA} ${SRCTOP}/release/packages/generate-ucl.lua \
		${_UCLVARS} \
		PKGNAME "${_pkg}" \
		PKGGENNAME "${WORLDPACKAGE}" \
		SRCDIR "${.CURDIR}" \
		"${.ALLSRC:M*.ucl.in}" ${.TARGET}

. endfor	# _pkg in ${_ALLPACKAGES}
.endif		# defined(WORLDPACKAGE)

# Stage the packages from objdir into repodir.

stagepackages: .PHONY
	mkdir -p ${_STAGEDIR}
	cp ${_ALLPACKAGES:S/^/${PKG_NAME_PREFIX}-/:S/$/-${PKG_VERSION}.pkg/} \
		${_STAGEDIR}

.SUFFIXES:	.plist .ucl .pkg
.PATH:		${_PKG_WORLDSTAGE}

.include <bsd.obj.mk>
