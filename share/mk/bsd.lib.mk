.include <bsd.init.mk>
.include <bsd.compiler.mk>
.include <bsd.linker.mk>
.include <bsd.compat.pre.mk>

__<bsd.lib.mk>__:	.NOTMAIN

.if defined(LIB_CXX) || defined(SHLIB_CXX)
_LD=	${CXX}
.else
_LD=	${CC}
.endif
.if defined(LIB_CXX)
LIB=	${LIB_CXX}
.endif
.if defined(SHLIB_CXX)
SHLIB=	${SHLIB_CXX}
.endif

LIB_PRIVATE=	${PRIVATELIB:Dprivate}
# Set up the variables controlling shared libraries.  After this section,
# SHLIB_NAME will be defined only if we are to create a shared library.
# SHLIB_LINK will be defined only if we are to create a link to it.
# INSTALL_PIC_ARCHIVE will be defined only if we are to create a PIC archive.
# BUILD_NOSSP_PIC_ARCHIVE will be defined only if we are to create a PIC archive.
.if defined(NO_PIC)
.undef SHLIB_NAME
.undef INSTALL_PIC_ARCHIVE
.undef BUILD_NOSSP_PIC_ARCHIVE
.else
.if !defined(SHLIB) && defined(LIB)
SHLIB=		${LIB}
.endif
.if !defined(SHLIB_NAME) && defined(SHLIB) && defined(SHLIB_MAJOR)
SHLIB_NAME=	lib${LIB_PRIVATE}${SHLIB}.so.${SHLIB_MAJOR}
.endif
.if defined(SHLIB_NAME) && !empty(SHLIB_NAME:M*.so.*)
SHLIB_LINK?=	${SHLIB_NAME:R}
.endif
SONAME?=	${SHLIB_NAME}
.endif

.if defined(CRUNCH_CFLAGS)
CFLAGS+=	${CRUNCH_CFLAGS}
.endif

.for _libcompat in ${_ALL_libcompats}
.if ${SHLIBDIR:M*/lib${_libcompat}} || ${SHLIBDIR:M*/lib${_libcompat}/*}
TAGS+=	lib${_libcompat}
.endif
.endfor

.if defined(NO_ROOT)
.if !defined(TAGS) || ! ${TAGS:Mpackage=*}
TAGS+=	package=${PACKAGE:Uutilities}
.endif

# By default, if PACKAGE=foo, then the native runtime libraries will go into
# the FreeBSD-foo package, and subpackages will be created for -dev, -lib32,
# and so on.  If LIB_PACKAGE is set, then we also create a subpackage for
# runtime libraries with a -lib suffix.  This is used when a package has
# libraries and some other content (e.g., executables) to allow consumers to
# depend on the libraries.
.if defined(LIB_PACKAGE) && ! ${TAGS:Mlib*}
.if !defined(PACKAGE)
.error LIB_PACKAGE cannot be used without PACKAGE
.endif

LIB_TAG_ARGS=	${TAG_ARGS},lib
.else
LIB_TAG_ARGS=	${TAG_ARGS}
.endif

TAG_ARGS=	-T ${TAGS:ts,:[*]}

DBG_TAG_ARGS=	${TAG_ARGS},dbg
# Usually we want to put development files (e.g., static libraries) into a
# separate -dev packages but for a few cases, like tests, that's not wanted,
# so allow the caller to disable it by setting NO_DEV_PACKAGE.
.if !defined(NO_DEV_PACKAGE)
DEV_TAG_ARGS=	${TAG_ARGS},dev
.else
DEV_TAG_ARGS=	${TAG_ARGS}
.endif

.endif	# defined(NO_ROOT)

# By default, put library manpages in the -dev subpackage, since they're not
# usually interesting if the development files aren't installed.   For pages
# that should be installed in the base package, define a new MANNODEV group.
# Note that bsd.man.mk ignores this setting if MANSPLITPKG is enabled: then
# manpages are always installed in the -man subpackage.
MANSUBPACKAGE?=	-dev
MANGROUPS?=	MAN
MANGROUPS+=	MANNODEV
MANNODEVSUBPACKAGE=

# ELF hardening knobs
.if ${MK_BIND_NOW} != "no"
LDFLAGS+= -Wl,-znow
.endif
.if ${LINKER_TYPE} != "mac"
.if ${MK_RELRO} == "no"
LDFLAGS+= -Wl,-znorelro
.else
LDFLAGS+= -Wl,-zrelro
.endif
.endif
.if ${MK_RETPOLINE} != "no"
.if ${COMPILER_FEATURES:Mretpoline} && ${LINKER_FEATURES:Mretpoline}
CFLAGS+= -mretpoline
CXXFLAGS+= -mretpoline
LDFLAGS+= -Wl,-zretpolineplt
.else
.warning Retpoline requested but not supported by compiler or linker
.endif
.endif
# LLD sensibly defaults to -znoexecstack, so do the same for BFD
LDFLAGS.bfd+= -Wl,-znoexecstack
.if ${MK_BRANCH_PROTECTION} != "no"
CFLAGS+=  -mbranch-protection=standard
.if ${LINKER_FEATURES:Mbti-report} && defined(BTI_REPORT_ERROR)
LDFLAGS+= -Wl,-zbti-report=error
.endif
.endif

# Initialize stack variables on function entry
.if ${OPT_INIT_ALL} != "none"
.if ${COMPILER_FEATURES:Minit-all}
CFLAGS+= -ftrivial-auto-var-init=${OPT_INIT_ALL}
CXXFLAGS+= -ftrivial-auto-var-init=${OPT_INIT_ALL}
.if ${OPT_INIT_ALL} == "zero" && ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} < 160000
CFLAGS+= -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang
CXXFLAGS+= -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang
.endif
.else
.warning INIT_ALL (${OPT_INIT_ALL}) requested but not supported by compiler
.endif
.endif

# Zero used registers on return (mitigate some ROP)
.if ${MK_ZEROREGS} != "no"
.if ${COMPILER_FEATURES:Mzeroregs}
ZEROREG_TYPE?= used
CFLAGS+= -fzero-call-used-regs=${ZEROREG_TYPE}
CXXFLAGS+= -fzero-call-used-regs=${ZEROREG_TYPE}
.endif
.endif

# bsd.sanitizer.mk is not installed, so don't require it (e.g. for ports).
.sinclude "bsd.sanitizer.mk"

.if ${MACHINE_CPUARCH} == "riscv" && ${LINKER_FEATURES:Mriscv-relaxations} == ""
CFLAGS += -mno-relax
.endif

.include <bsd.libnames.mk>

.include <bsd.suffixes-extra.mk>

_LIBDIR:=${LIBDIR}
_SHLIBDIR:=${SHLIBDIR}

.if defined(SHLIB_NAME)
.if ${MK_DEBUG_FILES} != "no"
SHLIB_NAME_FULL=${SHLIB_NAME}.full
DEBUGFILE= ${SHLIB_NAME}.debug
# Use ${DEBUGDIR} for base system debug files, else .debug subdirectory
.if ${_SHLIBDIR} == "/boot" ||\
    ${SHLIBDIR:C%/lib(/.*)?$%/lib%} == "/lib" ||\
    ${SHLIBDIR:C%/usr/lib(32|exec)?(/.*)?%/usr/lib%} == "/usr/lib" ||\
    ${SHLIBDIR:C%/usr/tests(/.*)?%/usr/tests%} == "/usr/tests"
DEBUGFILEDIR=${DEBUGDIR}${_SHLIBDIR}
.else
DEBUGFILEDIR=${_SHLIBDIR}/.debug
.endif
.if !exists(${DESTDIR}${DEBUGFILEDIR})
DEBUGMKDIR=
.endif
.else
SHLIB_NAME_FULL=${SHLIB_NAME}
.endif
.endif

.include <bsd.symver.mk>

# Allow libraries to specify their own version map or have it
# automatically generated (see bsd.symver.mk above).
.if !empty(VERSION_MAP)
${SHLIB_NAME_FULL}:	${VERSION_MAP}
LDFLAGS+=	-Wl,--version-script=${VERSION_MAP}

# Ideally we'd always enable --no-undefined-version (default for lld >= 16),
# but we have several symbols in our version maps that may or may not exist,
# depending on compile-time defines and that needs to be handled first.
.if ${MK_UNDEFINED_VERSION} == "no"
LDFLAGS+=	-Wl,--no-undefined-version
.else
LDFLAGS+=	-Wl,--undefined-version
.endif
.endif

.if defined(LIB) && !empty(LIB) || defined(SHLIB_NAME)
OBJS+=		${SRCS:N*.h:${OBJS_SRCS_FILTER:ts:}:S/$/.o/}
BCOBJS+=	${SRCS:N*.[hsS]:N*.asm:${OBJS_SRCS_FILTER:ts:}:S/$/.bco/g}
LLOBJS+=	${SRCS:N*.[hsS]:N*.asm:${OBJS_SRCS_FILTER:ts:}:S/$/.llo/g}
CLEANFILES+=	${OBJS} ${BCOBJS} ${LLOBJS} ${STATICOBJS}
.endif

.if defined(LIB) && !empty(LIB)
.if defined(STATIC_LDSCRIPT)
_STATICLIB_SUFFIX=	_real
.endif
_LIBS=		lib${LIB_PRIVATE}${LIB}${_STATICLIB_SUFFIX}.a

lib${LIB_PRIVATE}${LIB}${_STATICLIB_SUFFIX}.a: ${OBJS} ${STATICOBJS}
	@${ECHO} Building static ${LIB} library
	@rm -f ${.TARGET}
	${AR} ${ARFLAGS} ${.TARGET} ${OBJS} ${STATICOBJS} ${ARADD}
.endif

.if !defined(INTERNALLIB)

.if defined(LLVM_LINK)
lib${LIB_PRIVATE}${LIB}.bc: ${BCOBJS}
	${LLVM_LINK} -o ${.TARGET} ${BCOBJS}

lib${LIB_PRIVATE}${LIB}.ll: ${LLOBJS}
	${LLVM_LINK} -S -o ${.TARGET} ${LLOBJS}

CLEANFILES+=	lib${LIB_PRIVATE}${LIB}.bc lib${LIB_PRIVATE}${LIB}.ll
.endif

.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
SOBJS+=		${OBJS:.o=.pico}
DEPENDOBJS+=	${SOBJS}
CLEANFILES+=	${SOBJS}
.endif

.if defined(SHLIB_NAME)
_LIBS+=		${SHLIB_NAME}

SOLINKOPTS+=	-shared -Wl,-x
.if defined(LD_FATAL_WARNINGS) && ${LD_FATAL_WARNINGS} == "no"
SOLINKOPTS+=	-Wl,--no-fatal-warnings
.else
SOLINKOPTS+=	-Wl,--fatal-warnings
.endif
SOLINKOPTS+=	-Wl,--warn-shared-textrel

.if target(beforelinking)
beforelinking: ${SOBJS}
${SHLIB_NAME_FULL}: beforelinking
.endif

.if defined(SHLIB_LINK)
.if defined(SHLIB_LDSCRIPT) && !empty(SHLIB_LDSCRIPT) && exists(${.CURDIR}/${SHLIB_LDSCRIPT})
${SHLIB_LINK:R}.ld: ${.CURDIR}/${SHLIB_LDSCRIPT}
	sed -e 's,@@SHLIB@@,${_SHLIBDIR}/${SHLIB_NAME},g' \
	    -e 's,@@LIBDIR@@,${_LIBDIR},g' \
	    ${.ALLSRC} > ${.TARGET}

${SHLIB_NAME_FULL}: ${SHLIB_LINK:R}.ld
CLEANFILES+=	${SHLIB_LINK:R}.ld
.endif
CLEANFILES+=	${SHLIB_LINK}
.endif

${SHLIB_NAME_FULL}: ${SOBJS}
	@${ECHO} Building shared library ${SHLIB_NAME}
	@rm -f ${SHLIB_NAME} ${SHLIB_LINK}
.if defined(SHLIB_LINK) && !commands(${SHLIB_LINK:R}.ld) && ${MK_DEBUG_FILES} == "no"
	# Note: This uses ln instead of ${INSTALL_LIBSYMLINK} since we are in OBJDIR
	@${LN:Uln} -fs ${SHLIB_NAME} ${SHLIB_LINK}
.endif
	${_LD:N${CCACHE_BIN}} ${LDFLAGS} ${SSP_CFLAGS} ${SOLINKOPTS} \
	    -o ${.TARGET} -Wl,-soname,${SONAME} ${SOBJS} ${LDADD}
.if ${MK_CTF} != "no"
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${SOBJS}
.endif

.if ${MK_DEBUG_FILES} != "no"
CLEANFILES+=	${SHLIB_NAME_FULL} ${DEBUGFILE}
${SHLIB_NAME}: ${SHLIB_NAME_FULL} ${DEBUGFILE}
	${OBJCOPY} --strip-debug --add-gnu-debuglink=${DEBUGFILE} \
	    ${SHLIB_NAME_FULL} ${.TARGET}
.if defined(SHLIB_LINK) && !commands(${SHLIB_LINK:R}.ld)
	# Note: This uses ln instead of ${INSTALL_LIBSYMLINK} since we are in OBJDIR
	@${LN:Uln} -fs ${SHLIB_NAME} ${SHLIB_LINK}
.endif

${DEBUGFILE}: ${SHLIB_NAME_FULL}
	${OBJCOPY} --only-keep-debug ${SHLIB_NAME_FULL} ${.TARGET}
.endif
.endif #defined(SHLIB_NAME)

.if defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
_LIBS+=		lib${LIB_PRIVATE}${LIB}_pic.a

lib${LIB_PRIVATE}${LIB}_pic.a: ${SOBJS}
	@${ECHO} Building special pic ${LIB} library
	@rm -f ${.TARGET}
	${AR} ${ARFLAGS} ${.TARGET} ${SOBJS} ${ARADD}
.endif

.if defined(BUILD_NOSSP_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
NOSSPSOBJS+=	${OBJS:.o=.nossppico}
DEPENDOBJS+=	${NOSSPSOBJS}
CLEANFILES+=	${NOSSPSOBJS}
_LIBS+=		lib${LIB_PRIVATE}${LIB}_nossp_pic.a

lib${LIB_PRIVATE}${LIB}_nossp_pic.a: ${NOSSPSOBJS}
	@${ECHO} Building special nossp pic ${LIB} library
	@rm -f ${.TARGET}
	${AR} ${ARFLAGS} ${.TARGET} ${NOSSPSOBJS} ${ARADD}
.endif

.endif # !defined(INTERNALLIB)

.if defined(INTERNALLIB) && ${MK_PIE} != "no" && defined(LIB) && !empty(LIB)
PIEOBJS+=	${OBJS:.o=.pieo}
DEPENDOBJS+=	${PIEOBJS}
CLEANFILES+=	${PIEOBJS}

_LIBS+=		lib${LIB_PRIVATE}${LIB}_pie.a

lib${LIB_PRIVATE}${LIB}_pie.a: ${PIEOBJS}
	@${ECHO} Building pie ${LIB} library
	@rm -f ${.TARGET}
	${AR} ${ARFLAGS} ${.TARGET} ${PIEOBJS} ${ARADD}
.endif

.if defined(_SKIP_BUILD)
all:
.else
.if defined(_LIBS) && !empty(_LIBS)
all: ${_LIBS}
.endif

.if ${MK_MAN} != "no" && !defined(LIBRARIES_ONLY)
all: all-man
.endif
.endif

CLEANFILES+=	${_LIBS}

_EXTRADEPEND:
.if !defined(NO_EXTRADEPEND) && defined(SHLIB_NAME)
.if defined(DPADD) && !empty(DPADD)
	echo ${SHLIB_NAME_FULL}: ${DPADD} >> ${DEPENDFILE}
.endif
.endif

.if !target(install)

INSTALLFLAGS+= -C
.if defined(PRECIOUSLIB)
.if !defined(NO_FSCHG)
SHLINSTALLFLAGS+= -fschg
.endif
.endif
# Install libraries with -S to avoid risk of modifying in-use libraries when
# installing to a running system.  It is safe to avoid this for NO_ROOT builds
# that are only creating an image.
#
# XXX: Since Makefile.inc1 ends up building lib/libc both as part of
# _startup_libs and as part of _generic_libs it ends up getting installed a
# second time during the parallel build, and although the .WAIT in lib/Makefile
# stops that mattering for lib, other directories like secure/lib are built in
# parallel at the top level and are unaffected by that, so can sometimes race
# with the libc.so.7 reinstall and see a missing or corrupt file. Ideally the
# build system would be fixed to not build/install libc to WORLDTMP the second
# time round, but for now using -S ensures the install is atomic and thus we
# never see a broken intermediate state, so use it even for NO_ROOT builds.
.if !defined(NO_SAFE_LIBINSTALL) #&& !defined(NO_ROOT)
SHLINSTALLFLAGS+= -S
SHLINSTALLSYMLINKFLAGS+= -S
.endif

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor
_SHLINSTALLFLAGS:=	${SHLINSTALLFLAGS}
_SHLINSTALLSYMLINKFLAGS:= ${SHLINSTALLSYMLINKFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_SHLINSTALLFLAGS:=	${_SHLINSTALLFLAGS${ie}}
.endfor

.if defined(PCFILES)
.for pcfile in ${PCFILES}
installpcfiles: installpcfiles-${pcfile}

installpcfiles-${pcfile}: ${pcfile}
	${INSTALL} ${DEV_TAG_ARGS} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} \
	    ${.ALLSRC} ${DESTDIR}${LIBDATADIR}/pkgconfig/
.endfor
.endif
installpcfiles: .PHONY

.if !defined(INTERNALLIB)
realinstall: _libinstall installpcfiles _debuginstall
.ORDER: beforeinstall _libinstall _debuginstall
_libinstall:
.if defined(LIB) && !empty(LIB) && ${MK_INSTALLLIB} != "no"
	${INSTALL} ${DEV_TAG_ARGS} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB_PRIVATE}${LIB}${_STATICLIB_SUFFIX}.a ${DESTDIR}${_LIBDIR}/
.endif
.if defined(SHLIB_NAME)
	${INSTALL} ${LIB_TAG_ARGS} ${STRIP} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${_SHLINSTALLFLAGS} \
	    ${SHLIB_NAME} ${DESTDIR}${_SHLIBDIR}/
.if defined(SHLIB_LINK)
.if commands(${SHLIB_LINK:R}.ld)
	${INSTALL} ${DEV_TAG_ARGS} -S -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${SHLIB_LINK:R}.ld \
	    ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.for _SHLIB_LINK_LINK in ${SHLIB_LDSCRIPT_LINKS}
	${INSTALL_LIBSYMLINK} ${_SHLINSTALLSYMLINKFLAGS} ${LIB_TAG_ARGS} \
	    ${SHLIB_LINK} ${DESTDIR}${_LIBDIR}/${_SHLIB_LINK_LINK}
.endfor
.else
.if ${_SHLIBDIR} == ${_LIBDIR}
.if ${SHLIB_LINK:Mlib*}
	${INSTALL_RSYMLINK} ${_SHLINSTALLSYMLINKFLAGS} ${DEV_TAG_ARGS} \
	    ${SHLIB_NAME} ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.else
	${INSTALL_RSYMLINK} ${_SHLINSTALLSYMLINKFLAGS} ${LIB_TAG_ARGS} \
	    ${DESTDIR}${_SHLIBDIR}/${SHLIB_NAME} \
	    ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.endif
.else
.if ${SHLIB_LINK:Mlib*}
	${INSTALL_RSYMLINK} ${_SHLINSTALLSYMLINKFLAGS} ${DEV_TAG_ARGS} \
	    ${DESTDIR}${_SHLIBDIR}/${SHLIB_NAME} ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.else
	${INSTALL_RSYMLINK} ${_SHLINSTALLSYMLINKFLAGS} ${LIB_TAG_ARGS} \
	    ${DESTDIR}${_SHLIBDIR}/${SHLIB_NAME} ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.endif
.if exists(${DESTDIR}${_LIBDIR}/${SHLIB_NAME})
	-chflags noschg ${DESTDIR}${_LIBDIR}/${SHLIB_NAME}
	rm -f ${DESTDIR}${_LIBDIR}/${SHLIB_NAME}
.endif
.endif # _SHLIBDIR == _LIBDIR
.endif # SHLIB_LDSCRIPT
.endif # SHLIB_LINK
.endif # SHIB_NAME
.if defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
	${INSTALL} ${DEV_TAG_ARGS} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_pic.a ${DESTDIR}${_LIBDIR}/
.endif
.endif # !defined(INTERNALLIB)

.if !defined(LIBRARIES_ONLY)
.include <bsd.nls.mk>
.include <bsd.confs.mk>
.include <bsd.files.mk>
#No need to install header for INTERNALLIB
.if !defined(INTERNALLIB)
.include <bsd.incs.mk>
.endif
.endif

LINKOWN?=	${LIBOWN}
LINKGRP?=	${LIBGRP}
LINKMODE?=	${LIBMODE}
SYMLINKOWN?=	${LIBOWN}
SYMLINKGRP?=	${LIBGRP}
.if !defined(NO_DEV_PACKAGE)
LINKTAGS=	dev${_COMPAT_TAG}
.else
LINKTAGS=	${_COMPAT_TAG}
.endif
.include <bsd.links.mk>

.if ${MK_MAN} != "no" && !defined(LIBRARIES_ONLY)
realinstall: maninstall
.ORDER: beforeinstall maninstall
.endif

.endif

.if ${MK_MAN} != "no" && !defined(LIBRARIES_ONLY)
.include <bsd.man.mk>
.endif

.if defined(LIB) && !empty(LIB)
OBJS_DEPEND_GUESS+= ${SRCS:M*.h}
.endif
.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
.for _S in ${SRCS:N*.[hly]}
OBJS_DEPEND_GUESS.${_S:${OBJS_SRCS_FILTER:ts:}}.pico+=	${_S}
.endfor
.endif
.if defined(BUILD_NOSSP_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
.for _S in ${SRCS:N*.[hly]}
OBJS_DEPEND_GUESS.${_S:${OBJS_SRCS_FILTER:ts:}}.nossppico+=	${_S}
.endfor
.endif

.if defined(HAS_TESTS)
MAKE+=			MK_MAKE_CHECK_USE_SANDBOX=yes
SUBDIR_TARGETS+=	check
TESTS_LD_LIBRARY_PATH+=	${.OBJDIR}
.endif

.include <bsd.debug.mk>
.include <bsd.dep.mk>
.include <bsd.clang-analyze.mk>
.include <bsd.obj.mk>
.include <bsd.sys.mk>
