# $FreeBSD$
#
# This file contains common settings used for building FreeBSD
# sources.

# Enable various levels of compiler warning checks.  These may be
# overridden (e.g. if using a non-gcc compiler) by defining MK_WARNS=no.

# for GCC:   https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
# for clang: https://clang.llvm.org/docs/DiagnosticsReference.html

.include <bsd.compiler.mk>

# the default is gnu99 for now
CSTD?=		gnu99

.if ${CSTD} == "c89" || ${CSTD} == "c90"
CFLAGS+=	-std=iso9899:1990
.elif ${CSTD} == "c94" || ${CSTD} == "c95"
CFLAGS+=	-std=iso9899:199409
.elif ${CSTD} == "c99"
CFLAGS+=	-std=iso9899:1999
.else # CSTD
CFLAGS+=	-std=${CSTD}
.endif # CSTD

.if !empty(CXXSTD)
CXXFLAGS+=	-std=${CXXSTD}
.endif

# This gives the Makefile we're evaluating at the top-level a chance to set
# WARNS.  If it doesn't do so, we may freely pull a DEFAULTWARNS if it's set
# and use that.  This allows us to default WARNS to 6 for src builds without
# needing to set the default in various Makefile.inc.
.if !defined(WARNS) && defined(DEFAULTWARNS)
WARNS=	${DEFAULTWARNS}
.endif

# -pedantic is problematic because it also imposes namespace restrictions
#CFLAGS+=	-pedantic
.if defined(WARNS)
.if ${WARNS} >= 1
CWARNFLAGS+=	-Wsystem-headers
.if ${MK_WERROR} != "no" && ${MK_WERROR.${COMPILER_TYPE}:Uyes} != "no"
CWARNFLAGS+=	-Werror
.endif # ${MK_WERROR} != "no" && ${MK_WERROR.${COMPILER_TYPE}:Uyes} != "no"
.endif # WARNS >= 1
.if ${WARNS} >= 2
CWARNFLAGS+=	-Wall -Wno-format-y2k
.endif # WARNS >= 2
.if ${WARNS} >= 3
CWARNFLAGS+=	-W -Wno-unused-parameter -Wstrict-prototypes\
		-Wmissing-prototypes -Wpointer-arith
.endif # WARNS >= 3
.if ${WARNS} >= 4
CWARNFLAGS+=	-Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow\
		-Wunused-parameter
.if !defined(NO_WCAST_ALIGN) && !defined(NO_WCAST_ALIGN.${COMPILER_TYPE})
CWARNFLAGS+=	-Wcast-align
.endif # !NO_WCAST_ALIGN !NO_WCAST_ALIGN.${COMPILER_TYPE}
.endif # WARNS >= 4
.if ${WARNS} >= 6
CWARNFLAGS+=	-Wchar-subscripts -Wnested-externs -Wredundant-decls\
		-Wold-style-definition
.if !defined(NO_WMISSING_VARIABLE_DECLARATIONS)
CWARNFLAGS.clang+=	-Wmissing-variable-declarations
.endif
.if !defined(NO_WTHREAD_SAFETY)
CWARNFLAGS.clang+=	-Wthread-safety
.endif
.endif # WARNS >= 6
.if ${WARNS} >= 2 && ${WARNS} <= 4
# XXX Delete -Wuninitialized by default for now -- the compiler doesn't
# XXX always get it right.
CWARNFLAGS+=	-Wno-uninitialized
.endif # WARNS >=2 && WARNS <= 4
CWARNFLAGS+=	-Wno-pointer-sign
# Clang has more warnings enabled by default, and when using -Wall, so if WARNS
# is set to low values, these have to be disabled explicitly.
.if ${WARNS} <= 6
CWARNFLAGS.clang+=	-Wno-empty-body -Wno-string-plus-int
CWARNFLAGS.clang+=	-Wno-unused-const-variable
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 130000
CWARNFLAGS.clang+=	-Wno-error=unused-but-set-variable
.endif
.endif # WARNS <= 6
.if ${WARNS} <= 3
CWARNFLAGS.clang+=	-Wno-tautological-compare -Wno-unused-value\
		-Wno-parentheses-equality -Wno-unused-function -Wno-enum-conversion
CWARNFLAGS.clang+=	-Wno-unused-local-typedef
CWARNFLAGS.clang+=	-Wno-address-of-packed-member
.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 90100
CWARNFLAGS.gcc+=	-Wno-address-of-packed-member
.endif
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 70000 && \
    ${MACHINE_CPUARCH} == "arm" && !${MACHINE_ARCH:Marmv[67]*}
CWARNFLAGS.clang+=	-Wno-atomic-alignment
.endif
.endif # WARNS <= 3
.if ${WARNS} <= 2
CWARNFLAGS.clang+=	-Wno-switch -Wno-switch-enum -Wno-knr-promoted-parameter
.endif # WARNS <= 2
.if ${WARNS} <= 1
CWARNFLAGS.clang+=	-Wno-parentheses
.endif # WARNS <= 1
.if defined(NO_WARRAY_BOUNDS)
CWARNFLAGS.clang+=	-Wno-array-bounds
.endif # NO_WARRAY_BOUNDS
.if defined(NO_WMISLEADING_INDENTATION) && \
    ((${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 100000) || \
      ${COMPILER_TYPE} == "gcc")
CWARNFLAGS+=		-Wno-misleading-indentation
.endif # NO_WMISLEADING_INDENTATION
.endif # WARNS

.if defined(FORMAT_AUDIT)
WFORMAT=	1
.endif # FORMAT_AUDIT
.if defined(WFORMAT)
.if ${WFORMAT} > 0
#CWARNFLAGS+=	-Wformat-nonliteral -Wformat-security -Wno-format-extra-args
CWARNFLAGS+=	-Wformat=2 -Wno-format-extra-args
.if ${WARNS} <= 3
CWARNFLAGS.clang+=	-Wno-format-nonliteral
.endif # WARNS <= 3
.if ${MK_WERROR} != "no" && ${MK_WERROR.${COMPILER_TYPE}:Uyes} != "no"
CWARNFLAGS+=	-Werror
.endif # ${MK_WERROR} != "no" && ${MK_WERROR.${COMPILER_TYPE}:Uyes} != "no"
.endif # WFORMAT > 0
.endif # WFORMAT
.if defined(NO_WFORMAT) || defined(NO_WFORMAT.${COMPILER_TYPE})
CWARNFLAGS+=	-Wno-format
.endif # NO_WFORMAT || NO_WFORMAT.${COMPILER_TYPE}

# GCC
# We should clean up warnings produced with these flags.
# They were originally added as a quick hack to enable gcc5/6.
# The base system requires at least GCC 6.4, but some ports
# use this file with older compilers.  Request an exprun
# before changing these.
.if ${COMPILER_TYPE} == "gcc"
# GCC 5.2.0
.if ${COMPILER_VERSION} >= 50200
CWARNFLAGS+=	-Wno-error=address			\
		-Wno-error=array-bounds			\
		-Wno-error=attributes			\
		-Wno-error=bool-compare			\
		-Wno-error=cast-align			\
		-Wno-error=clobbered			\
		-Wno-error=deprecated-declarations	\
		-Wno-error=enum-compare			\
		-Wno-error=extra			\
		-Wno-error=logical-not-parentheses	\
		-Wno-error=strict-aliasing		\
		-Wno-error=uninitialized		\
		-Wno-error=unused-but-set-variable	\
		-Wno-error=unused-function		\
		-Wno-error=unused-value
.endif

# GCC 6.1.0
.if ${COMPILER_VERSION} >= 60100
CWARNFLAGS+=	-Wno-error=empty-body			\
		-Wno-error=maybe-uninitialized		\
		-Wno-error=nonnull-compare		\
		-Wno-error=redundant-decls		\
		-Wno-error=shift-negative-value		\
		-Wno-error=tautological-compare		\
		-Wno-error=unused-const-variable
.endif

# GCC 7.1.0
.if ${COMPILER_VERSION} >= 70100
CWARNFLAGS+=	-Wno-error=bool-operation		\
		-Wno-error=deprecated			\
		-Wno-error=expansion-to-defined		\
		-Wno-error=format-overflow		\
		-Wno-error=format-truncation		\
		-Wno-error=implicit-fallthrough		\
		-Wno-error=int-in-bool-context		\
		-Wno-error=memset-elt-size		\
		-Wno-error=noexcept-type		\
		-Wno-error=nonnull			\
		-Wno-error=pointer-compare		\
		-Wno-error=stringop-overflow
.endif

# GCC 8.1.0
.if ${COMPILER_VERSION} >= 80100
CWARNFLAGS+=	-Wno-error=aggressive-loop-optimizations	\
		-Wno-error=cast-function-type			\
		-Wno-error=catch-value				\
		-Wno-error=multistatement-macros		\
		-Wno-error=restrict				\
		-Wno-error=sizeof-pointer-memaccess		\
		-Wno-error=stringop-truncation
.endif

# GCC 9.2.0
.if ${COMPILER_VERSION} >= 90200
.if ${MACHINE_ARCH} == "i386"
CWARNFLAGS+=	-Wno-error=overflow
.endif
.endif

# GCC's own arm_neon.h triggers various warnings
.if ${MACHINE_CPUARCH} == "aarch64"
CWARNFLAGS+=	-Wno-system-headers
.endif
.endif	# gcc

# How to handle FreeBSD custom printf format specifiers.
.if ${COMPILER_TYPE} == "clang"
FORMAT_EXTENSIONS=	-D__printf__=__freebsd_kprintf__
.else
FORMAT_EXTENSIONS=	-fformat-extensions
.endif

.if defined(IGNORE_PRAGMA)
CWARNFLAGS+=	-Wno-unknown-pragmas
.endif # IGNORE_PRAGMA

# This warning is utter nonsense
CFLAGS+=	-Wno-format-zero-length

.if ${COMPILER_TYPE} == "clang"
# The headers provided by clang are incompatible with the FreeBSD headers.
# If the version of clang is not one that has been patched to omit the
# incompatible headers, we need to compile with -nobuiltininc and add the
# resource dir to the end of the search paths. This ensures that headers such as
# immintrin.h are still found but stddef.h, etc. are picked up from FreeBSD.
#
# XXX: This is a hack to support complete external installs of clang while
# we work to synchronize our decleration guards with those in the clang tree.
.if ${MK_CLANG_BOOTSTRAP:Uno} == "no" && \
    ${COMPILER_RESOURCE_DIR} != "unknown" && !defined(BOOTSTRAPPING)
CFLAGS+=-nobuiltininc -idirafter ${COMPILER_RESOURCE_DIR}/include
.endif
.endif

CLANG_OPT_SMALL= -mstack-alignment=8 -mllvm -inline-threshold=3
.if ${COMPILER_VERSION} < 130000
CLANG_OPT_SMALL+= -mllvm -simplifycfg-dup-ret
.endif
CLANG_OPT_SMALL+= -mllvm -enable-load-pre=false
CFLAGS.clang+=	 -Qunused-arguments
# The libc++ headers use c++11 extensions.  These are normally silenced because
# they are treated as system headers, but we explicitly disable that warning
# suppression when building the base system to catch bugs in our headers.
# Eventually we'll want to start building the base system C++ code as C++11,
# but not yet.
CXXFLAGS.clang+=	 -Wno-c++11-extensions

.if ${MK_SSP} != "no"
# Don't use -Wstack-protector as it breaks world with -Werror.
SSP_CFLAGS?=	-fstack-protector-strong
CFLAGS+=	${SSP_CFLAGS}
.endif # SSP

# Additional flags passed in CFLAGS and CXXFLAGS when MK_DEBUG_FILES is
# enabled.
DEBUG_FILES_CFLAGS?= -g -gz=zlib

# Allow user-specified additional warning flags, plus compiler and file
# specific flag overrides, unless we've overridden this...
.if ${MK_WARNS} != "no"
CFLAGS+=	${CWARNFLAGS:M*} ${CWARNFLAGS.${COMPILER_TYPE}}
CFLAGS+=	${CWARNFLAGS.${.IMPSRC:T}}
CXXFLAGS+=	${CXXWARNFLAGS:M*} ${CXXWARNFLAGS.${COMPILER_TYPE}}
CXXFLAGS+=	${CXXWARNFLAGS.${.IMPSRC:T}}
.endif

CFLAGS+=	 ${CFLAGS.${COMPILER_TYPE}}
CXXFLAGS+=	 ${CXXFLAGS.${COMPILER_TYPE}}

AFLAGS+=	${AFLAGS.${.IMPSRC:T}}
AFLAGS+=	${AFLAGS.${.TARGET:T}}
ACFLAGS+=	${ACFLAGS.${.IMPSRC:T}}
ACFLAGS+=	${ACFLAGS.${.TARGET:T}}
CFLAGS+=	${CFLAGS.${.IMPSRC:T}}
CXXFLAGS+=	${CXXFLAGS.${.IMPSRC:T}}

LDFLAGS+=	${LDFLAGS.${LINKER_TYPE}}

# Only allow .TARGET when not using PROGS as it has the same syntax
# per PROG which is ambiguous with this syntax. This is only needed
# for PROG_VARS vars.
#
# Some directories (currently just clang) also need to disable this since
# CFLAGS.${COMPILER_TYPE}, CFLAGS.${.IMPSRC:T} and CFLAGS.${.TARGET:T} all live
# in the same namespace, meaning that, for example, GCC builds of clang pick up
# CFLAGS.clang via CFLAGS.${.TARGET:T} and thus try to pass Clang-specific
# flags. Ideally the different sources of CFLAGS would be namespaced to avoid
# collisions.
.if !defined(_RECURSING_PROGS) && !defined(NO_TARGET_FLAGS)
.if ${MK_WARNS} != "no"
CFLAGS+=	${CWARNFLAGS.${.TARGET:T}}
.endif
CFLAGS+=	${CFLAGS.${.TARGET:T}}
CXXFLAGS+=	${CXXFLAGS.${.TARGET:T}}
LDFLAGS+=	${LDFLAGS.${.TARGET:T}}
LDADD+=		${LDADD.${.TARGET:T}}
LIBADD+=	${LIBADD.${.TARGET:T}}
.endif

.if defined(SRCTOP)
# Prevent rebuilding during install to support read-only objdirs.
.if ${.TARGETS:M*install*} == ${.TARGETS} && empty(.MAKE.MODE:Mmeta)
CFLAGS+=	ERROR-tried-to-rebuild-during-make-install
.endif
.endif

# Please keep this if in sync with kern.mk
.if ${LD} != "ld" && (${CC:[1]:H} != ${LD:[1]:H} || ${LD:[1]:T} != "ld")
# Add -fuse-ld=${LD} if $LD is in a different directory or not called "ld".
.if ${COMPILER_TYPE} == "clang"
# Note: Clang does not like relative paths for ld so we map ld.lld -> lld.
.if ${COMPILER_VERSION} >= 120000
LDFLAGS+=	--ld-path=${LD:[1]:S/^ld.//1W}
.else
LDFLAGS+=	-fuse-ld=${LD:[1]:S/^ld.//1W}
.endif
.else
# GCC does not support an absolute path for -fuse-ld so we just print this
# warning instead and let the user add the required symlinks.
# However, we can avoid this warning if -B is set appropriately (e.g. for
# CROSS_TOOLCHAIN=...-gcc).
.if !(${LD:[1]:T} == "ld" && ${CC:tw:M-B${LD:[1]:H}/})
.warning LD (${LD}) is not the default linker for ${CC} but -fuse-ld= is not supported
.endif
.endif
.endif

# Tell bmake not to mistake standard targets for things to be searched for
# or expect to ever be up-to-date.
PHONY_NOTMAIN = analyze afterdepend afterinstall all beforedepend beforeinstall \
		beforelinking build build-tools buildconfig buildfiles \
		buildincludes check checkdpadd clean cleandepend cleandir \
		cleanobj configure depend distclean distribute exe \
		files html includes install installconfig installdirs \
		installfiles installincludes lint obj objlink objs objwarn \
		realinstall tags whereobj

# we don't want ${PROG} to be PHONY
.PHONY: ${PHONY_NOTMAIN:N${PROG:U}}
.NOTMAIN: ${PHONY_NOTMAIN:Nall}

.if ${MK_STAGING} != "no"
.if defined(_SKIP_BUILD) || (!make(all) && !make(clean*) && !make(*clean))
_SKIP_STAGING?= yes
.endif
.if ${_SKIP_STAGING:Uno} == "yes"
staging stage_libs stage_files stage_as stage_links stage_symlinks:
.else
# allow targets like beforeinstall to be leveraged
DESTDIR= ${STAGE_OBJTOP}
.export DESTDIR

.if target(beforeinstall)
.if !empty(_LIBS) || (${MK_STAGING_PROG} != "no" && !defined(INTERNALPROG))
staging: beforeinstall
.endif
.endif

# normally only libs and includes are staged
.if ${MK_STAGING_PROG} != "no" && !defined(INTERNALPROG)
STAGE_DIR.prog= ${STAGE_OBJTOP}${BINDIR}

.if !empty(PROG)
.if defined(PROGNAME)
STAGE_AS_SETS+= prog
STAGE_AS_${PROG}= ${PROGNAME}
stage_as.prog: ${PROG}
.else
STAGE_SETS+= prog
stage_files.prog: ${PROG}
STAGE_TARGETS+= stage_files
.endif
.endif
.endif

.if !empty(_LIBS) && !defined(INTERNALLIB)
.if defined(SHLIBDIR) && ${SHLIBDIR} != ${LIBDIR} && ${_LIBS:Uno:M*.so.*} != ""
STAGE_SETS+= shlib
STAGE_DIR.shlib= ${STAGE_OBJTOP}${SHLIBDIR}
STAGE_FILES.shlib+= ${_LIBS:M*.so.*}
stage_files.shlib: ${_LIBS:M*.so.*}
.endif

.if defined(SHLIB_LINK) && commands(${SHLIB_LINK:R}.ld)
STAGE_AS_SETS+= ldscript
STAGE_AS.ldscript+= ${SHLIB_LINK:R}.ld
stage_as.ldscript: ${SHLIB_LINK:R}.ld
STAGE_DIR.ldscript = ${STAGE_LIBDIR}
STAGE_AS_${SHLIB_LINK:R}.ld:= ${SHLIB_LINK}
NO_SHLIB_LINKS=
.endif

.if target(stage_files.shlib)
stage_libs: ${_LIBS}
.if defined(DEBUG_FLAGS) && target(${SHLIB_NAME}.symbols)
stage_files.shlib: ${SHLIB_NAME}.symbols
.endif
.else
stage_libs: ${_LIBS}
.endif
.if defined(SHLIB_NAME) && defined(DEBUG_FLAGS) && target(${SHLIB_NAME}.symbols)
stage_libs: ${SHLIB_NAME}.symbols
.endif

.endif

.if !empty(INCS) || !empty(INCSGROUPS) && target(buildincludes)
.if !defined(NO_BEFOREBUILD_INCLUDES)
stage_includes: buildincludes
beforebuild: stage_includes
.endif
.endif

.for t in stage_libs stage_files stage_as
.if target($t)
STAGE_TARGETS+= $t
.endif
.endfor

.if !empty(STAGE_AS_SETS)
STAGE_TARGETS+= stage_as
.endif

.if !empty(STAGE_TARGETS) || (${MK_STAGING_PROG} != "no" && !defined(INTERNALPROG))

.if !empty(LINKS)
STAGE_TARGETS+= stage_links
.if ${MAKE_VERSION} < 20131001
stage_links.links: ${_LIBS} ${PROG}
.endif
STAGE_SETS+= links
STAGE_LINKS.links= ${LINKS}
.endif

.if !empty(SYMLINKS)
STAGE_TARGETS+= stage_symlinks
STAGE_SETS+= links
STAGE_SYMLINKS.links= ${SYMLINKS}
.endif

.endif

.include <meta.stage.mk>
.endif
.endif

.if defined(META_TARGETS)
.for _tgt in ${META_TARGETS}
.if target(${_tgt})
${_tgt}: ${META_DEPS}
.endif
.endfor
.endif
