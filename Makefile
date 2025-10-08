#
#
# The common user-driven targets are (for a complete list, see build(7)):
#
# universe            - *Really* build *everything* (buildworld and
#                       all kernels on all architectures).  Define
#                       MAKE_JUST_KERNELS or WITHOUT_WORLDS to only build kernels,
#                       MAKE_JUST_WORLDS or WITHOUT_KERNELS to only build userland.
# tinderbox           - Same as universe, but presents a list of failed build
#                       targets and exits with an error if there were any.
# worlds	      - Same as universe, except just makes the worlds.
# kernels	      - Same as universe, except just makes the kernels.
# buildworld          - Rebuild *everything*, including glue to help do
#                       upgrades.
# installworld        - Install everything built by "buildworld".
# world               - buildworld + installworld, no kernel.
# buildkernel         - Rebuild the kernel and the kernel-modules.
# installkernel       - Install the kernel and the kernel-modules.
# installkernel.debug
# reinstallkernel     - Reinstall the kernel and the kernel-modules.
# reinstallkernel.debug
# kernel              - buildkernel + installkernel.
# kernel-toolchain    - Builds the subset of world necessary to build a kernel
# kernel-toolchains   - Build kernel-toolchain for all universe targets.
# doxygen             - Build API documentation of the kernel, needs doxygen.
# checkworld          - Run test suite on installed world.
# check-old           - List obsolete directories/files/libraries.
# check-old-dirs      - List obsolete directories.
# check-old-files     - List obsolete files.
# check-old-libs      - List obsolete libraries.
# delete-old          - Delete obsolete directories/files.
# delete-old-dirs     - Delete obsolete directories.
# delete-old-files    - Delete obsolete files.
# delete-old-libs     - Delete obsolete libraries.
# list-old-dirs       - Raw list of possibly obsolete directories.
# list-old-files      - Raw list of possibly obsolete files.
# list-old-libs       - Raw list of possibly obsolete libraries.
# targets             - Print a list of supported TARGET/TARGET_ARCH pairs
#                       for world and kernel targets.
# toolchains          - Build a toolchain for all world and kernel targets.
# makeman             - Regenerate src.conf(5)
# sysent              - (Re)build syscall entries from syscalls.master.
# xdev                - xdev-build + xdev-install for the architecture
#                       specified with TARGET and TARGET_ARCH.
# xdev-build          - Build cross-development tools.
# xdev-install        - Install cross-development tools.
# xdev-links          - Create traditional links in /usr/bin for cc, etc
# native-xtools       - Create host binaries that produce target objects
#                       for use in qemu user-mode jails.  TARGET and
#                       TARGET_ARCH should be defined.
# native-xtools-install
#                     - Install the files to the given DESTDIR/NXTP where
#                       NXTP defaults to /nxb-bin.
#
# This makefile is simple by design. The FreeBSD make automatically reads
# the /usr/share/mk/sys.mk unless the -m argument is specified on the
# command line. By keeping this makefile simple, it doesn't matter too
# much how different the installed mk files are from those in the source
# tree. This makefile executes a child make process, forcing it to use
# the mk files from the source tree which are supposed to DTRT.
#
# Most of the user-driven targets (as listed above) are implemented in
# Makefile.inc1.  The exceptions are universe, tinderbox and targets.
#
# If you want to build your system from source, be sure that /usr/obj has
# at least 6 GB of disk space available.  A complete 'universe' build of
# r340283 (2018-11) required 167 GB of space.  ZFS lz4 compression
# achieved a 2.18x ratio, reducing actual space to 81 GB.
#
# For individuals wanting to build from the sources currently on their
# system, the simple instructions are:
#
# 1.  `cd /usr/src'  (or to the directory containing your source tree).
# 2.  Define `HISTORICAL_MAKE_WORLD' variable (see README).
# 3.  `make world'
#
# For individuals wanting to upgrade their sources (even if only a
# delta of a few days):
#
#  1.  `cd /usr/src'       (or to the directory containing your source tree).
#  2.  `make buildworld'
#  3.  `make buildkernel KERNCONF=YOUR_KERNEL_HERE'     (default is GENERIC).
#  4.  `make installkernel KERNCONF=YOUR_KERNEL_HERE'   (default is GENERIC).
#       [steps 3. & 4. can be combined by using the "kernel" target]
#  5.  `reboot'        (in single user mode: boot -s from the loader prompt).
#  6.  `etcupdate -p'
#  7.  `make installworld'
#  8.  `etcupdate -B'
#  9.  `make delete-old'
# 10.  `reboot'
# 11.  `make delete-old-libs' (in case no 3rd party program uses them anymore)
#
# For individuals wanting to build from source with GCC from ports, first
# install the appropriate GCC cross toolchain package:
#   `pkg install ${TARGET_ARCH}-gccN`
#
# Once you have installed the necessary cross toolchain, simply pass
# CROSS_TOOLCHAIN=${TARGET_ARCH}-gccN while building with the above steps,
# e.g., `make buildworld CROSS_TOOLCHAIN=amd64-gcc13`.
#
# The ${TARGET_ARCH}-gccN packages are provided as flavors of the
# devel/freebsd-gccN ports.
#
# See src/UPDATING `COMMON ITEMS' for more complete information.
#
# If TARGET=machine (e.g. powerpc64, arm64, ...) is specified you can
# cross build world for other machine types using the buildworld target,
# and once the world is built you can cross build a kernel using the
# buildkernel target.
#
# Define the user-driven targets. These are listed here in alphabetical
# order, but that's not important.
#
# Targets that begin with underscore are internal targets intended for
# developer convenience only.  They are intentionally not documented and
# completely subject to change without notice.
#
# For more information, see the build(7) manual page.
#

# Include jobs.mk early if we need it.
# It will turn:
# 	make buildworld-jobs
# into
# 	make -j${JOB_MAX} buildworld > ../buildworld.log 2>&1
#
.if make(*-jobs)
.include <jobs.mk>
.endif

.if defined(UNIVERSE_TARGET) || defined(MAKE_JUST_WORLDS) || defined(WITHOUT_KERNELS)
__DO_KERNELS=no
.endif
.if defined(MAKE_JUST_KERNELS) || defined(WITHOUT_WORLDS)
__DO_WORLDS=no
.endif
__DO_WORLDS?=yes
__DO_KERNELS?=yes

# This is included so CC is set to ccache for -V, and COMPILER_TYPE/VERSION can
# be cached for sub-makes. The need for CC is done with new make later in the
# build, and caching COMPILER_TYPE/VERSION is only an optimization. Also
# sinclude it to be friendlier to foreign OS hosted builds.
.sinclude <bsd.compiler.mk>

# Note: we use this awkward construct to be compatible with FreeBSD's
# old make used in 10.0 and 9.2 and earlier.
.if defined(MK_DIRDEPS_BUILD) && ${MK_DIRDEPS_BUILD} == "yes" && \
    !make(showconfig) && !make(print-dir)
# targets/Makefile plays the role of top-level
.include "targets/Makefile"
.else

.include "${.CURDIR}/share/mk/bsd.compat.pre.mk"

TGTS=	all all-man buildenv buildenvvars buildetc buildkernel buildworld \
	check check-old check-old-dirs check-old-files check-old-libs \
	checkdpadd checkworld clean cleandepend cleandir cleankernel \
	cleanworld cleanuniverse \
	delete-old delete-old-dirs delete-old-files delete-old-libs \
	depend distribute distributekernel distributekernel.debug \
	distributeworld distrib-dirs distribution doxygen \
	everything hier hierarchy install installcheck installetc installkernel \
	installkernel.debug packagekernel packageworld \
	reinstallkernel reinstallkernel.debug \
	installworld kernel-toolchain libraries maninstall \
	list-old-dirs list-old-files list-old-libs \
	obj objlink showconfig tags toolchain \
	makeman sysent \
	_cleanworldtmp _worldtmp _legacy _bootstrap-tools _cleanobj _obj \
	_build-tools _build-metadata _cross-tools _includes _libraries \
	builddtb xdev xdev-build xdev-install \
	xdev-links native-xtools native-xtools-install stageworld stagekernel \
	stage-packages stage-packages-kernel stage-packages-world stage-packages-source \
	create-packages-world create-packages-kernel \
	create-packages-kernel-repo create-packages-world-repo \
	create-packages-source create-packages \
	installconfig real-packages real-update-packages \
	sign-packages package-pkg print-dir test-system-compiler test-system-linker \
	test-includes

.for libcompat in ${_ALL_libcompats}
TGTS+=	build${libcompat} distribute${libcompat} install${libcompat}
.endfor

# These targets require a TARGET and TARGET_ARCH be defined.
XTGTS=	native-xtools native-xtools-install xdev xdev-build xdev-install \
	xdev-links

# XXX: r156740: This can't work since bsd.subdir.mk is not included ever.
# It will only work for SUBDIR_TARGETS in make.conf.
TGTS+=	${SUBDIR_TARGETS}

BITGTS=	files includes
BITGTS:=${BITGTS} ${BITGTS:S/^/build/} ${BITGTS:S/^/install/}
TGTS+=	${BITGTS}

# Only some targets are allowed to use meta mode.  Others get it
# disabled.  In some cases, such as 'install', meta mode can be dangerous
# as a cookie may be used to prevent redundant installations (such as
# for WORLDTMP staging).  For DESTDIR=/ we always want to install though.
# For other cases, such as delete-old-libs, meta mode may break
# the interactive tty prompt.  The safest route is to just whitelist
# the ones that benefit from it.
META_TGT_WHITELIST+= \
	_* buildfiles buildincludes buildkernel \
	buildworld everything kernel-toolchain kernel-toolchains kernel \
	kernels libraries native-xtools showconfig test-includes \
	test-system-compiler test-system-linker tinderbox toolchain \
	toolchains universe universe-toolchain world worlds xdev xdev-build

.for libcompat in ${_ALL_libcompats}
META_TGT_WHITELIST+=	build${libcompat}
.endfor

.ORDER: buildworld installworld
.ORDER: buildworld distrib-dirs
.ORDER: buildworld distribution
.ORDER: buildworld distribute
.ORDER: buildworld distributeworld
.ORDER: buildworld buildkernel
.ORDER: buildworld packages
.ORDER: buildworld update-packages
.ORDER: distrib-dirs distribute
.ORDER: distrib-dirs distributeworld
.ORDER: distrib-dirs installworld
.ORDER: distribution distribute
.ORDER: distributeworld distribute
.ORDER: distributeworld distribution
.ORDER: installworld distribute
.ORDER: installworld distribution
.ORDER: installworld installkernel
.ORDER: buildkernel installkernel
.ORDER: buildkernel installkernel.debug
.ORDER: buildkernel reinstallkernel
.ORDER: buildkernel reinstallkernel.debug
.ORDER: buildkernel packages
.ORDER: buildkernel update-packages
.ORDER: kernel-toolchain buildkernel

# Only sanitize PATH on FreeBSD.
# PATH may include tools that are required to cross-build
# on non-FreeBSD systems.
.if ${.MAKE.OS} == "FreeBSD"
PATH=	/sbin:/bin:/usr/sbin:/usr/bin
.endif
MAKEOBJDIRPREFIX?=	/usr/obj
_MAKEOBJDIRPREFIX!= /usr/bin/env -i PATH=${PATH:Q} ${MAKE} MK_AUTO_OBJ=no \
    ${.MAKEFLAGS:MMAKEOBJDIRPREFIX=*} __MAKE_CONF=${__MAKE_CONF} \
    SRCCONF=${SRCCONF} SRC_ENV_CONF= \
    -f /dev/null -V MAKEOBJDIRPREFIX dummy
.if !empty(_MAKEOBJDIRPREFIX) || !empty(.MAKEOVERRIDES:MMAKEOBJDIRPREFIX)
.error MAKEOBJDIRPREFIX can only be set in environment or src-env.conf(5),\
    not as a global (in make.conf(5) or src.conf(5)) or command-line variable.
.endif

# We often need to use the tree's version of make to build it.
.if !empty(.MAKE.MODE:Mmeta)
# 20160604 - support missing-meta,missing-filemon and performance improvements
WANT_MAKE_VERSION= 20160604
.else
# 20160220 - support .dinclude for FAST_DEPEND.
WANT_MAKE_VERSION= 20160220
.endif
.if defined(MYMAKE)
.error MYMAKE cannot be overridden, use as command name instead
.endif
MYMAKE=		${OBJROOT}make.${MACHINE}/bmake
.if defined(ALWAYS_BOOTSTRAP_MAKE) || \
    (defined(WANT_MAKE_VERSION) && ${MAKE_VERSION} < ${WANT_MAKE_VERSION})
NEED_MAKE_UPGRADE= t
.endif
.if defined(NEED_MAKE_UPGRADE)
. if exists(${MYMAKE})
SUB_MAKE:= ${MYMAKE} -m ${.CURDIR}/share/mk
. else
# It may not exist yet but we may cause it to.
SUB_MAKE= `test -x ${MYMAKE} && echo ${MYMAKE} || echo ${MAKE}` \
	-m ${.CURDIR}/share/mk
. endif
.else
SUB_MAKE= ${MAKE} -m ${.CURDIR}/share/mk
.endif

_MAKE=	PATH=${PATH:Q} MAKE_CMD="${MAKE}" ${SUB_MAKE} -f Makefile.inc1 \
	TARGET=${_TARGET} TARGET_ARCH=${_TARGET_ARCH} ${_MAKEARGS}

.if defined(MK_META_MODE) && ${MK_META_MODE} == "yes"
# Only allow meta mode for the whitelisted targets.  See META_TGT_WHITELIST
# above.  If overridden as a make argument then don't bother trying to
# disable it.
.if empty(.MAKEOVERRIDES:MMK_META_MODE)
.for _tgt in ${META_TGT_WHITELIST}
.if make(${_tgt})
_CAN_USE_META_MODE?= yes
.endif
.endfor
.if !defined(_CAN_USE_META_MODE)
_MAKE+=	MK_META_MODE=no
MK_META_MODE= no
.unexport META_MODE
.endif	# !defined(_CAN_USE_META_MODE)
.endif	# empty(.MAKEOVERRIDES:MMK_META_MODE)

.if ${MK_META_MODE} == "yes"
.if !exists(/dev/filemon) && !defined(NO_FILEMON) && !make(showconfig)
# Require filemon be loaded to provide a working incremental build
.error ${.newline}ERROR: The filemon module (/dev/filemon) is not loaded. \
    ${.newline}ERROR: WITH_META_MODE is enabled but requires filemon for an incremental build. \
    ${.newline}ERROR: 'kldload filemon' or pass -DNO_FILEMON to suppress this error.
.endif	# !exists(/dev/filemon) && !defined(NO_FILEMON)
.endif	# ${MK_META_MODE} == yes
.endif	# defined(MK_META_MODE) && ${MK_META_MODE} == yes

# Guess target architecture from target type, and vice versa, based on
# historic FreeBSD practice of tending to have TARGET == TARGET_ARCH
# expanding to TARGET == TARGET_CPUARCH in recent times, with known
# exceptions.
.if !defined(TARGET_ARCH) && defined(TARGET)
# T->TA mapping is usually TARGET with arm64 the odd man out
_TARGET_ARCH=	${TARGET:S/arm64/aarch64/:S/riscv/riscv64/:S/arm/armv7/}
.elif !defined(TARGET) && defined(TARGET_ARCH) && \
    ${TARGET_ARCH} != ${MACHINE_ARCH}
# TA->T mapping is accidentally CPUARCH with aarch64 the odd man out
_TARGET=	${TARGET_ARCH:${__TO_CPUARCH}:C/aarch64/arm64/}
.endif
.if defined(TARGET) && !defined(_TARGET)
_TARGET=${TARGET}
.endif
.if defined(TARGET_ARCH) && !defined(_TARGET_ARCH)
_TARGET_ARCH=${TARGET_ARCH}
.endif
# for historical compatibility for xdev targets
.if defined(XDEV)
_TARGET=	${XDEV}
.endif
.if defined(XDEV_ARCH)
_TARGET_ARCH=	${XDEV_ARCH}
.endif
# Some targets require a set TARGET/TARGET_ARCH, check before the default
# MACHINE and after the compatibility handling.
.if !defined(_TARGET) || !defined(_TARGET_ARCH)
${XTGTS}: _assert_target
.endif
# Otherwise, default to current machine type and architecture.
_TARGET?=	${MACHINE}
_TARGET_ARCH?=	${MACHINE_ARCH}

.if make(native-xtools*)
NXB_TARGET:=		${_TARGET}
NXB_TARGET_ARCH:=	${_TARGET_ARCH}
_TARGET=		${MACHINE}
_TARGET_ARCH=		${MACHINE_ARCH}
_MAKE+=			NXB_TARGET=${NXB_TARGET} \
			NXB_TARGET_ARCH=${NXB_TARGET_ARCH}
.endif

.if make(print-dir)
.SILENT:
.endif

_assert_target: .PHONY .MAKE
.for _tgt in ${XTGTS}
.if make(${_tgt})
	@echo "*** Error: Both TARGET and TARGET_ARCH must be defined for \"${_tgt}\" target"
	@false
.endif
.endfor

#
# Make sure we have an up-to-date make(1). Only world, buildworld and
# kernel-toolchain should do this as those are the initial targets used
# for upgrades. The user can define ALWAYS_CHECK_MAKE to have this check
# performed for all targets.
#
.if defined(ALWAYS_CHECK_MAKE)
${TGTS}: upgrade_checks
.else
buildworld: upgrade_checks
kernel-toolchain: upgrade_checks
.endif

#
# Handle the user-driven targets, using the source relative mk files.
#

tinderbox toolchains kernel-toolchains: .MAKE
${TGTS}: .PHONY .MAKE
	${_+_}@cd ${.CURDIR}; ${_MAKE} ${.TARGET}

# The historic default "all" target creates files which may cause stale
# or (in the cross build case) unlinkable results. Fail with an error
# when no target is given. The users can explicitly specify "all"
# if they want the historic behavior.
.MAIN:	_guard

_guard: .PHONY
	@echo
	@echo "Explicit target required.  Likely \"${SUBDIR_OVERRIDE:Dall:Ubuildworld}\" is wanted.  See build(7)."
	@echo
	@false

STARTTIME!= LC_ALL=C date
CHECK_TIME!= cmp=`mktemp`; find ${.CURDIR}/sys/sys/param.h -newer "$$cmp" && rm "$$cmp"; echo
.if !empty(CHECK_TIME)
.error check your date/time: ${STARTTIME}
.endif

.if defined(HISTORICAL_MAKE_WORLD) || defined(DESTDIR)
#
# world
#
# Attempt to rebuild and reinstall everything. This target is not to be
# used for upgrading an existing FreeBSD system, because the kernel is
# not included. One can argue that this target doesn't build everything
# then.
#
world: upgrade_checks .PHONY
	@echo "--------------------------------------------------------------"
	@echo ">>> make world started on ${STARTTIME}"
	@echo "--------------------------------------------------------------"
.if target(pre-world)
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> Making 'pre-world' target"
	@echo "--------------------------------------------------------------"
	${_+_}@cd ${.CURDIR}; ${_MAKE} pre-world
.endif
	${_+_}@cd ${.CURDIR}; ${_MAKE} buildworld
	${_+_}@cd ${.CURDIR}; ${_MAKE} installworld MK_META_MODE=no
.if target(post-world)
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> Making 'post-world' target"
	@echo "--------------------------------------------------------------"
	${_+_}@cd ${.CURDIR}; ${_MAKE} post-world
.endif
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> make world completed on `LC_ALL=C date`"
	@echo "                   (started ${STARTTIME})"
	@echo "--------------------------------------------------------------"
.else
world: .PHONY
	@echo "WARNING: make world will overwrite your existing FreeBSD"
	@echo "installation without also building and installing a new"
	@echo "kernel.  This can be dangerous.  Please read the handbook,"
	@echo "'Rebuilding world', for how to upgrade your system."
	@echo "Define DESTDIR to where you want to install FreeBSD,"
	@echo "including /, to override this warning and proceed as usual."
	@echo ""
	@echo "Bailing out now..."
	@false
.endif

#
# kernel
#
# Short hand for `make buildkernel installkernel'
#
kernel: buildkernel installkernel .PHONY

#
# Perform a few tests to determine if the installed tools are adequate
# for building the world.
#
upgrade_checks: .PHONY
.if defined(NEED_MAKE_UPGRADE)
	@${_+_}(cd ${.CURDIR} && ${MAKE} bmake)
.elif exists(${MYMAKE:H})
	@echo "Removing stale bmake(1)"
	rm -r ${MYMAKE:H}
.endif

#
# Upgrade make(1) to the current version using the installed
# headers, libraries and tools.  Also, allow the location of
# the system bsdmake-like utility to be overridden.
#
MMAKEENV=	\
		DESTDIR= \
		INSTALL="sh ${.CURDIR}/tools/install.sh"
MMAKE=		${MMAKEENV} ${MAKE} \
		OBJTOP=${MYMAKE:H}/obj \
		OBJROOT='$${OBJTOP}/' \
		MAKEOBJDIRPREFIX= \
		MK_MAN=no -DNO_SHARED \
		-DNO_CPU_CFLAGS MK_WERROR=no \
		-DNO_SUBDIR \
		DESTDIR= PROGNAME=${MYMAKE:T}

bmake: .PHONY
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> Building an up-to-date ${.TARGET}(1)"
	@echo "--------------------------------------------------------------"
	${_+_}@cd ${.CURDIR}/usr.bin/${.TARGET}; \
		${MMAKE} obj; \
		${MMAKE} depend; \
		${MMAKE} all; \
		${MMAKE} install DESTDIR=${MYMAKE:H} BINDIR=

regress: .PHONY
	@echo "'make regress' has been renamed 'make check'" | /usr/bin/fmt
	@false

tinderbox toolchains kernel-toolchains kernels worlds: upgrade_checks

tinderbox: .PHONY
	@cd ${.CURDIR}; ${SUB_MAKE} DOING_TINDERBOX=YES universe

toolchains: .PHONY
	@cd ${.CURDIR}; ${SUB_MAKE} UNIVERSE_TARGET=toolchain universe

kernel-toolchains: .PHONY
	@cd ${.CURDIR}; ${SUB_MAKE} UNIVERSE_TARGET=kernel-toolchain universe

kernels: .PHONY
	@cd ${.CURDIR}; ${SUB_MAKE} universe -DWITHOUT_WORLDS

worlds: .PHONY
	@cd ${.CURDIR}; ${SUB_MAKE} UNIVERSE_TARGET=buildworld universe

packages update-packages: .PHONY
	${_+_}@cd ${.CURDIR}; ${_MAKE} DISTDIR=/ ${.TARGET}

#
# universe
#
# Attempt to rebuild *everything* for all supported architectures,
# with a reasonable chance of success, regardless of how old your
# existing system is.
#
.if make(universe) || make(universe_kernels) || make(tinderbox) || \
    make(targets) || make(universe-toolchain)
#
# Don't build rarely used, semi-supported architectures unless requested.
#
.if defined(EXTRA_TARGETS)
EXTRA_ARCHES_powerpc=	powerpc powerpcspe
.endif
TARGETS?= ${TARGET_MACHINE_LIST}
_UNIVERSE_TARGETS=	${TARGETS}
.for target in ${TARGETS}
TARGET_ARCHES_${target}= ${MACHINE_ARCH_LIST_${target}}
.endfor

.if defined(USE_GCC_TOOLCHAINS)
_DEFAULT_GCC_VERSION=	gcc14
_GCC_VERSION=		${"${USE_GCC_TOOLCHAINS:Mgcc*}" != "":?${USE_GCC_TOOLCHAINS}:${_DEFAULT_GCC_VERSION}}
TOOLCHAINS_amd64=	amd64-${_GCC_VERSION}
TOOLCHAINS_arm=		armv7-${_GCC_VERSION}
TOOLCHAINS_arm64=	aarch64-${_GCC_VERSION}
TOOLCHAINS_i386=	i386-${_GCC_VERSION}
TOOLCHAINS_powerpc=	powerpc64-${_GCC_VERSION}
TOOLCHAINS_riscv=	riscv64-${_GCC_VERSION}
.endif

# If a target is using an external toolchain, set MAKE_PARAMS to enable use
# of the toolchain.  If the external toolchain is missing, exclude the target
# from universe.
.for target in ${_UNIVERSE_TARGETS}
.if !empty(TOOLCHAINS_${target})
.for toolchain in ${TOOLCHAINS_${target}}
.if !exists(/usr/local/share/toolchains/${toolchain}.mk)
_UNIVERSE_TARGETS:= ${_UNIVERSE_TARGETS:N${target}}
universe: universe_${toolchain}_skip .PHONY
universe_epilogue: universe_${toolchain}_skip .PHONY
universe_${toolchain}_skip: universe_prologue .PHONY
	@echo ">> ${target} skipped - install ${toolchain} port or package to build"
.endif
.endfor
.for arch in ${TARGET_ARCHES_${target}}
TOOLCHAIN_${arch}?=	${TOOLCHAINS_${target}:[1]}
MAKE_PARAMS_${arch}?=	CROSS_TOOLCHAIN=${TOOLCHAIN_${arch}}
.endfor
.endif
.endfor

UNIVERSE_TARGET?=	buildworld
KERNSRCDIR?=		${.CURDIR}/sys

.if ${.MAKE.OS} == "FreeBSD"
UNIVERSE_TOOLCHAIN_TARGET?=		${MACHINE}
UNIVERSE_TOOLCHAIN_TARGET_ARCH?=	${MACHINE_ARCH}
.else
# MACHINE/MACHINE_ARCH may not follow the same naming as us (e.g. x86_64 vs
# amd64) on non-FreeBSD. Rather than attempt to sanitise it, arbitrarily use
# amd64 as the default universe toolchain target.
UNIVERSE_TOOLCHAIN_TARGET?=		amd64
UNIVERSE_TOOLCHAIN_TARGET_ARCH?=	amd64
.endif

targets:	.PHONY
	@echo "Supported TARGET/TARGET_ARCH pairs for world and kernel targets"
.for target in ${TARGETS}
.for target_arch in ${TARGET_ARCHES_${target}}
	@echo "    ${target}/${target_arch}"
.endfor
.endfor

.if defined(DOING_TINDERBOX)
FAILFILE=${.CURDIR}/_.tinderbox.failed
MAKEFAIL=tee -a ${FAILFILE}
.else
MAKEFAIL=cat
.endif

universe_prologue:  upgrade_checks
universe: universe_prologue
universe_prologue: .PHONY
	@echo "--------------------------------------------------------------"
	@echo ">>> make universe started on ${STARTTIME}"
	@echo "--------------------------------------------------------------"
.if defined(DOING_TINDERBOX)
	@rm -f ${FAILFILE}
.endif

universe-toolchain: .PHONY universe_prologue
	@echo "--------------------------------------------------------------"
	@echo "> Toolchain bootstrap started on `LC_ALL=C date`"
	@echo "--------------------------------------------------------------"
	${_+_}@cd ${.CURDIR}; \
	    env PATH=${PATH:Q} ${SUB_MAKE} ${JFLAG} kernel-toolchain \
	    TARGET=${UNIVERSE_TOOLCHAIN_TARGET} \
	    TARGET_ARCH=${UNIVERSE_TOOLCHAIN_TARGET_ARCH} \
	    OBJTOP="${HOST_OBJTOP}" \
	    WITHOUT_SYSTEM_COMPILER=yes \
	    WITHOUT_SYSTEM_LINKER=yes \
	    TOOLS_PREFIX_UNDEF= \
	    kernel-toolchain \
	    MK_LLVM_TARGET_ALL=yes \
	    > _.${.TARGET} 2>&1 || \
	    (echo "${.TARGET} failed," \
	    "check _.${.TARGET} for details" | \
	    ${MAKEFAIL}; false)
	@if [ ! -e "${HOST_OBJTOP}/tmp/usr/bin/cc" ]; then \
	    echo "Missing host compiler at ${HOST_OBJTOP}/tmp/usr/bin/cc?" >&2; \
	    false; \
	fi
	@if [ ! -e "${HOST_OBJTOP}/tmp/usr/bin/ld" ]; then \
	    echo "Missing host linker at ${HOST_OBJTOP}/tmp/usr/bin/ld?" >&2; \
	    false; \
	fi
	@echo "--------------------------------------------------------------"
	@echo "> Toolchain bootstrap completed on `LC_ALL=C date`"
	@echo "--------------------------------------------------------------"

.for target in ${_UNIVERSE_TARGETS}
universe: universe_${target}
universe_epilogue: universe_${target}
universe_${target}: universe_${target}_prologue .PHONY
universe_${target}_prologue: universe_prologue .PHONY
	@echo ">> ${target} started on `LC_ALL=C date`"
universe_${target}_worlds: .PHONY

.if !make(targets) && !make(universe-toolchain)
.for target_arch in ${TARGET_ARCHES_${target}}
.if !defined(_need_clang_${target}_${target_arch})
_need_clang_${target}_${target_arch} != \
	env TARGET=${target} TARGET_ARCH=${target_arch} \
	${SUB_MAKE} -C ${.CURDIR} -f Makefile.inc1 test-system-compiler \
	    ${MAKE_PARAMS_${target_arch}} -V MK_CLANG_BOOTSTRAP 2>/dev/null || \
	    echo unknown
.export _need_clang_${target}_${target_arch}
.endif
.if !defined(_need_lld_${target}_${target_arch})
_need_lld_${target}_${target_arch} != \
	env TARGET=${target} TARGET_ARCH=${target_arch} \
	${SUB_MAKE} -C ${.CURDIR} -f Makefile.inc1 test-system-linker \
	    ${MAKE_PARAMS_${target_arch}} -V MK_LLD_BOOTSTRAP 2>/dev/null || \
	    echo unknown
.export _need_lld_${target}_${target_arch}
.endif
# Setup env for each arch to use the one clang.
.if defined(_need_clang_${target}_${target_arch}) && \
    ${_need_clang_${target}_${target_arch}} == "yes"
# No check on existing XCC or CROSS_BINUTILS_PREFIX, etc, is needed since
# we use the test-system-compiler logic to determine if clang needs to be
# built.  It will be no from that logic if already using an external
# toolchain or /usr/bin/cc.
# XXX: Passing HOST_OBJTOP into the PATH would allow skipping legacy,
#      bootstrap-tools, and cross-tools.  Need to ensure each tool actually
#      supports all TARGETS though.
# For now we only pass UNIVERSE_TOOLCHAIN_PATH which will be added at the end
# of STRICTTMPPATH to ensure that the target-specific binaries come first.
MAKE_PARAMS_${target_arch}+= \
	XCC="${HOST_OBJTOP}/tmp/usr/bin/cc" \
	XCXX="${HOST_OBJTOP}/tmp/usr/bin/c++" \
	XCPP="${HOST_OBJTOP}/tmp/usr/bin/cpp" \
	UNIVERSE_TOOLCHAIN_PATH=${HOST_OBJTOP}/tmp/usr/bin
.endif
.if defined(_need_lld_${target}_${target_arch}) && \
    ${_need_lld_${target}_${target_arch}} == "yes"
MAKE_PARAMS_${target_arch}+= \
	XLD="${HOST_OBJTOP}/tmp/usr/bin/ld"
.endif
.endfor
.endif	# !make(targets)

.if ${__DO_WORLDS} == "yes"
universe_${target}_done: universe_${target}_worlds .PHONY
.for target_arch in ${TARGET_ARCHES_${target}}
universe_${target}_worlds: universe_${target}_${target_arch} .PHONY
.if (defined(_need_clang_${target}_${target_arch}) && \
    ${_need_clang_${target}_${target_arch}} == "yes") || \
    (defined(_need_lld_${target}_${target_arch}) && \
    ${_need_lld_${target}_${target_arch}} == "yes")
universe_${target}_${target_arch}: universe-toolchain
universe_${target}_prologue: universe-toolchain
.endif
universe_${target}_${target_arch}: universe_${target}_prologue .MAKE .PHONY
	@echo ">> ${target}.${target_arch} ${UNIVERSE_TARGET} started on `LC_ALL=C date`"
	@(cd ${.CURDIR} && env __MAKE_CONF=/dev/null \
	    ${SUB_MAKE} ${JFLAG} ${UNIVERSE_TARGET} \
	    TARGET=${target} \
	    TARGET_ARCH=${target_arch} \
	    ${MAKE_PARAMS_${target_arch}} \
	    > _.${target}.${target_arch}.${UNIVERSE_TARGET} 2>&1 || \
	    (echo "${target}.${target_arch} ${UNIVERSE_TARGET} failed," \
	    "check _.${target}.${target_arch}.${UNIVERSE_TARGET} for details" | \
	    ${MAKEFAIL}))
	@echo ">> ${target}.${target_arch} ${UNIVERSE_TARGET} completed on `LC_ALL=C date`"
.endfor
.endif # ${__DO_WORLDS} == "yes"

.if ${__DO_KERNELS} == "yes"
universe_${target}_done: universe_${target}_kernels .PHONY
universe_${target}_kernels: universe_${target}_worlds .PHONY
universe_${target}_kernels: universe_${target}_prologue .MAKE .PHONY
	@cd ${.CURDIR}; ${SUB_MAKE} ${.MAKEFLAGS} TARGET=${target} \
	    universe_kernels
.endif # ${__DO_KERNELS} == "yes"

# Tell the user the worlds and kernels have completed
universe_${target}: universe_${target}_done
universe_${target}_done:
	@echo ">> ${target} completed on `LC_ALL=C date`"
.endfor
.if make(universe_kernconfs) || make(universe_kernels)
.if !defined(TARGET)
TARGET!=	uname -m
.endif
universe_kernels_prologue: .PHONY
	@echo ">> ${TARGET} kernels started on `LC_ALL=C date`"
universe_kernels: universe_kernconfs .PHONY
	@echo ">> ${TARGET} kernels completed on `LC_ALL=C date`"
.if defined(MAKE_ALL_KERNELS)
_THINNER=cat
.elif defined(MAKE_LINT_KERNELS)
_THINNER=grep 'LINT' || true
.else
_THINNER=xargs grep -L "^.NO_UNIVERSE" || true
.endif
KERNCONFS!=	cd ${KERNSRCDIR}/${TARGET}/conf && \
		find [[:upper:][:digit:]]*[[:upper:][:digit:]] \
		-type f -maxdepth 0 \
		! -name DEFAULTS ! -name NOTES | \
		${_THINNER}
universe_kernconfs: universe_kernels_prologue .PHONY
.for kernel in ${KERNCONFS}
TARGET_ARCH_${kernel}!=	cd ${KERNSRCDIR}/${TARGET}/conf && \
	env PATH=${HOST_OBJTOP}/tmp/legacy/bin:${PATH:Q} \
	config -m ${KERNSRCDIR}/${TARGET}/conf/${kernel} 2> /dev/null | \
	grep -v WARNING: | cut -f 2
.if empty(TARGET_ARCH_${kernel})
.error Target architecture for ${TARGET}/conf/${kernel} unknown.  config(8) likely too old.
.endif
universe_kernconfs_${TARGET_ARCH_${kernel}}: universe_kernconf_${TARGET}_${kernel}
universe_kernconf_${TARGET}_${kernel}: .MAKE
	@echo ">> ${TARGET}.${TARGET_ARCH_${kernel}} ${kernel} kernel started on `LC_ALL=C date`"
	@(cd ${.CURDIR} && env __MAKE_CONF=/dev/null \
	    ${SUB_MAKE} ${JFLAG} buildkernel \
	    TARGET=${TARGET} \
	    TARGET_ARCH=${TARGET_ARCH_${kernel}} \
	    ${MAKE_PARAMS_${TARGET_ARCH_${kernel}}} \
	    KERNCONF=${kernel} \
	    > _.${TARGET}.${kernel} 2>&1 || \
	    (echo "${TARGET} ${kernel} kernel failed," \
	    "check _.${TARGET}.${kernel} for details"| ${MAKEFAIL}))
	@echo ">> ${TARGET}.${TARGET_ARCH_${kernel}} ${kernel} kernel completed on `LC_ALL=C date`"
.endfor
.for target_arch in ${TARGET_ARCHES_${TARGET}}
universe_kernconfs: universe_kernconfs_${target_arch} .PHONY
universe_kernconfs_${target_arch}:
.endfor
.endif	# make(universe_kernels)
universe: universe_epilogue
universe_epilogue: .PHONY
	@echo "--------------------------------------------------------------"
	@echo ">>> make universe completed on `LC_ALL=C date`"
	@echo "                      (started ${STARTTIME})"
	@echo "--------------------------------------------------------------"
.if defined(DOING_TINDERBOX)
	@if [ -e ${FAILFILE} ] ; then \
		echo "Tinderbox failed:" ;\
		cat ${FAILFILE} ;\
		exit 1 ;\
	fi
.endif
.endif

# This makefile does not run in meta mode
.MAKE.MODE= normal
# Normally the things we run from here don't either.
# Using -DWITH_META_MODE
# we can buildworld with meta files created which are useful 
# for debugging, but without any of the rest of a meta mode build.
MK_DIRDEPS_BUILD= no
MK_STAGING= no
# tell meta.autodep.mk to not even think about updating anything.
UPDATE_DEPENDFILE= NO
.if !make(showconfig)
.export MK_DIRDEPS_BUILD MK_STAGING UPDATE_DEPENDFILE
.endif

.if make(universe)
# we do not want a failure of one branch abort all.
MAKE_JOB_ERROR_TOKEN= no
.export MAKE_JOB_ERROR_TOKEN
.endif

.endif				# DIRDEPS_BUILD
