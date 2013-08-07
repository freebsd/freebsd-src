#
# $FreeBSD$
#
# The user-driven targets are:
#
# universe            - *Really* build *everything* (buildworld and
#                       all kernels on all architectures).
# tinderbox           - Same as universe, but presents a list of failed build
#                       targets and exits with an error if there were any.
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
# update              - Convenient way to update your source tree(s).
# check-old           - List obsolete directories/files/libraries.
# check-old-dirs      - List obsolete directories.
# check-old-files     - List obsolete files.
# check-old-libs      - List obsolete libraries.
# delete-old          - Delete obsolete directories/files.
# delete-old-dirs     - Delete obsolete directories.
# delete-old-files    - Delete obsolete files.
# delete-old-libs     - Delete obsolete libraries.
# targets             - Print a list of supported TARGET/TARGET_ARCH pairs
#                       for world and kernel targets.
# toolchains          - Build a toolchain for all world and kernel targets.
# 
# "quick" way to test all kernel builds:
# 	_jflag=`sysctl -n hw.ncpu`
# 	_jflag=$(($_jflag * 2))
# 	[ $_jflag -gt 12 ] && _jflag=12
# 	make universe -DMAKE_JUST_KERNELS JFLAG=-j${_jflag}
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
# If you want to build your system from source be sure that /usr/obj has
# at least 1GB of diskspace available.  A complete 'universe' build requires
# about 15GB of space.
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
#  6.  `mergemaster -p'
#  7.  `make installworld'
#  8.  `mergemaster'		(you may wish to use -i, along with -U or -F).
#  9.  `make delete-old'
# 10.  `reboot'
# 11.  `make delete-old-libs' (in case no 3rd party program uses them anymore)
#
# See src/UPDATING `COMMON ITEMS' for more complete information.
#
# If TARGET=machine (e.g. ia64, sparc64, ...) is specified you can
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
TGTS=	all all-man buildenv buildenvvars buildkernel buildworld \
	check-old check-old-dirs check-old-files check-old-libs \
	checkdpadd clean cleandepend cleandir \
	delete-old delete-old-dirs delete-old-files delete-old-libs \
	depend distribute distributekernel distributekernel.debug \
	distributeworld distrib-dirs distribution doxygen \
	everything hier hierarchy install installcheck installkernel \
	installkernel.debug packagekernel packageworld \
	reinstallkernel reinstallkernel.debug \
	installworld kernel-toolchain libraries lint maninstall \
	obj objlink regress rerelease showconfig tags toolchain update \
	_worldtmp _legacy _bootstrap-tools _cleanobj _obj \
	_build-tools _cross-tools _includes _libraries _depend \
	build32 builddtb distribute32 install32 xdev xdev-build xdev-install \

TGTS+=	${SUBDIR_TARGETS}

BITGTS=	files includes
BITGTS:=${BITGTS} ${BITGTS:S/^/build/} ${BITGTS:S/^/install/}
TGTS+=	${BITGTS}

.ORDER: buildworld installworld
.ORDER: buildworld distributeworld
.ORDER: buildworld buildkernel
.ORDER: buildkernel installkernel
.ORDER: buildkernel installkernel.debug
.ORDER: buildkernel reinstallkernel
.ORDER: buildkernel reinstallkernel.debug

PATH=	/sbin:/bin:/usr/sbin:/usr/bin
MAKEOBJDIRPREFIX?=	/usr/obj
_MAKEOBJDIRPREFIX!= /usr/bin/env -i PATH=${PATH} ${MAKE} \
    ${.MAKEFLAGS:MMAKEOBJDIRPREFIX=*} __MAKE_CONF=${__MAKE_CONF} \
    -f /dev/null -V MAKEOBJDIRPREFIX dummy
.if !empty(_MAKEOBJDIRPREFIX)
.error MAKEOBJDIRPREFIX can only be set in environment, not as a global\
	(in make.conf(5)) or command-line variable.
.endif

# We often need to use the tree's version of make to build it.
# Choices add to complexity though.
# We cannot blindly use a make which may not be the one we want
# so be exlicit - until all choice is removed.
.if !defined(WITHOUT_BMAKE)
WANT_MAKE=	bmake
.else
WANT_MAKE=	fmake
.endif
MYMAKE=		${MAKEOBJDIRPREFIX}${.CURDIR}/make.${MACHINE}/${WANT_MAKE}
.if defined(.PARSEDIR)
HAVE_MAKE=	bmake
.else
HAVE_MAKE=	fmake
.endif
.if exists(${MYMAKE})
SUB_MAKE:= ${MYMAKE} -m ${.CURDIR}/share/mk
.elif ${WANT_MAKE} != ${HAVE_MAKE} || ${WANT_MAKE} != "bmake"
# It may not exist yet but we may cause it to.
# In the case of fmake, upgrade_checks may cause a newer version to be built.
SUB_MAKE= `test -x ${MYMAKE} && echo ${MYMAKE} || echo ${MAKE}` \
	-m ${.CURDIR}/share/mk
.else
SUB_MAKE= ${MAKE} -m ${.CURDIR}/share/mk
.endif

_MAKE=	PATH=${PATH} ${SUB_MAKE} -f Makefile.inc1 TARGET=${_TARGET} TARGET_ARCH=${_TARGET_ARCH}

# Guess machine architecture from machine type, and vice versa.
.if !defined(TARGET_ARCH) && defined(TARGET)
_TARGET_ARCH=	${TARGET:S/pc98/i386/}
.elif !defined(TARGET) && defined(TARGET_ARCH) && \
    ${TARGET_ARCH} != ${MACHINE_ARCH}
_TARGET=		${TARGET_ARCH:C/mips(n32|64)?(el)?/mips/:C/arm(v6)?(eb)?/arm/}
.endif
# Legacy names, for another transition period mips:mips(n32|64)?eb -> mips:mips\1
.if defined(TARGET) && defined(TARGET_ARCH) && \
    ${TARGET} == "mips" && ${TARGET_ARCH:Mmips*eb}
_TARGET_ARCH=		${TARGET_ARCH:C/eb$//}
.warning "TARGET_ARCH of ${TARGET_ARCH} is deprecated in favor of ${_TARGET_ARCH}"
.endif
.if defined(TARGET) && ${TARGET} == "mips" && defined(TARGET_BIG_ENDIAN)
.warning "TARGET_BIG_ENDIAN is no longer necessary for MIPS.  Big-endian is not the default."
.endif
# arm with TARGET_BIG_ENDIAN -> armeb
.if defined(TARGET_ARCH) && ${TARGET_ARCH} == "arm" && defined(TARGET_BIG_ENDIAN)
.warning "TARGET_ARCH of arm with TARGET_BIG_ENDIAN is deprecated.  use armeb"
_TARGET_ARCH=armeb
.endif
.if defined(TARGET) && !defined(_TARGET)
_TARGET=${TARGET}
.endif
.if defined(TARGET_ARCH) && !defined(_TARGET_ARCH)
_TARGET_ARCH=${TARGET_ARCH}
.endif
# Otherwise, default to current machine type and architecture.
_TARGET?=	${MACHINE}
_TARGET_ARCH?=	${MACHINE_ARCH}

#
# Make sure we have an up-to-date make(1). Only world and buildworld
# should do this as those are the initial targets used for upgrades.
# The user can define ALWAYS_CHECK_MAKE to have this check performed
# for all targets.
#
.if defined(ALWAYS_CHECK_MAKE)
${TGTS}: upgrade_checks
.else
buildworld: upgrade_checks
.endif

#
# This 'cleanworld' target is not included in TGTS, because it is not a
# recursive target.  All of the work for it is done right here.   It is
# expected that BW_CANONICALOBJDIR == the CANONICALOBJDIR as would be
# created by bsd.obj.mk, except that we don't want to .include that file
# in this makefile.  
#
# In the following, the first 'rm' in a series will usually remove all
# files and directories.  If it does not, then there are probably some
# files with file flags set, so this unsets them and tries the 'rm' a
# second time.  There are situations where this target will be cleaning
# some directories via more than one method, but that duplication is
# needed to correctly handle all the possible situations.  Removing all
# files without file flags set in the first 'rm' instance saves time,
# because 'chflags' will need to operate on fewer files afterwards.
#
BW_CANONICALOBJDIR:=${MAKEOBJDIRPREFIX}${.CURDIR}
cleanworld:
.if ${.CURDIR} == ${.OBJDIR} || ${.CURDIR}/obj == ${.OBJDIR}
.if exists(${BW_CANONICALOBJDIR}/)
	-rm -rf ${BW_CANONICALOBJDIR}/*
	-chflags -R 0 ${BW_CANONICALOBJDIR}
	rm -rf ${BW_CANONICALOBJDIR}/*
.endif
	#   To be safe in this case, fall back to a 'make cleandir'
	${_+_}@cd ${.CURDIR}; ${_MAKE} cleandir
.else
	-rm -rf ${.OBJDIR}/*
	-chflags -R 0 ${.OBJDIR}
	rm -rf ${.OBJDIR}/*
.endif

#
# Handle the user-driven targets, using the source relative mk files.
#

.if empty(.MAKEFLAGS:M-n)
# skip this for -n to avoid changing previous behavior of 
# 'make -n buildworld' etc.
${TGTS}: .MAKE
.endif

${TGTS}:
	${_+_}@cd ${.CURDIR}; ${_MAKE} ${.TARGET}

# Set a reasonable default
.MAIN:	all

STARTTIME!= LC_ALL=C date
CHECK_TIME!= find ${.CURDIR}/sys/sys/param.h -mtime -0s ; echo
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
world: upgrade_checks
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
	${_+_}@cd ${.CURDIR}; ${_MAKE} -B installworld
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
world:
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
kernel: buildkernel installkernel

#
# Perform a few tests to determine if the installed tools are adequate
# for building the world.
#
# Note: if we ever need to care about the version of bmake, simply testing
# MAKE_VERSION against a required version should suffice.
#
upgrade_checks:
.if ${HAVE_MAKE} != ${WANT_MAKE}
	@(cd ${.CURDIR} && ${MAKE} ${WANT_MAKE:S,^f,,})
.elif ${WANT_MAKE} == "fmake"
	@if ! (cd ${.CURDIR}/tools/build/make_check && \
	    PATH=${PATH} ${BINMAKE} obj >/dev/null 2>&1 && \
	    PATH=${PATH} ${BINMAKE} >/dev/null 2>&1); \
	then \
	    (cd ${.CURDIR} && ${MAKE} make); \
	fi
.endif

#
# Upgrade make(1) to the current version using the installed
# headers, libraries and tools.  Also, allow the location of
# the system bsdmake-like utility to be overridden.
#
MMAKEENV=	MAKEOBJDIRPREFIX=${MYMAKE:H} \
		DESTDIR= \
		INSTALL="sh ${.CURDIR}/tools/install.sh"
MMAKE=		${MMAKEENV} ${MAKE} \
		-D_UPGRADING \
		-DNOMAN -DNO_MAN -DNOSHARED -DNO_SHARED \
		-DNO_CPU_CFLAGS -DNO_WERROR

make bmake: .PHONY
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> Building an up-to-date make(1)"
	@echo "--------------------------------------------------------------"
	${_+_}@cd ${.CURDIR}/usr.bin/${.TARGET}; \
		${MMAKE} obj && \
		${MMAKE} depend && \
		${MMAKE} all && \
		${MMAKE} install DESTDIR=${MYMAKE:H} BINDIR= PROGNAME=${MYMAKE:T}

tinderbox:
	@cd ${.CURDIR} && ${MAKE} DOING_TINDERBOX=YES universe

toolchains:
	@cd ${.CURDIR} && ${MAKE} UNIVERSE_TARGET=toolchain universe

kernel-toolchains:
	@cd ${.CURDIR} && ${MAKE} UNIVERSE_TARGET=kernel-toolchain universe

#
# universe
#
# Attempt to rebuild *everything* for all supported architectures,
# with a reasonable chance of success, regardless of how old your
# existing system is.
#
.if make(universe) || make(universe_kernels) || make(tinderbox) || make(targets)
TARGETS?=amd64 arm i386 ia64 mips pc98 powerpc sparc64
TARGET_ARCHES_arm?=	arm armeb armv6 armv6eb
TARGET_ARCHES_mips?=	mipsel mips mips64el mips64 mipsn32
TARGET_ARCHES_powerpc?=	powerpc powerpc64
TARGET_ARCHES_pc98?=	i386
.for target in ${TARGETS}
TARGET_ARCHES_${target}?= ${target}
.endfor

.if defined(UNIVERSE_TARGET)
MAKE_JUST_WORLDS=	YES
.else
UNIVERSE_TARGET?=	buildworld
.endif
KERNSRCDIR?=		${.CURDIR}/sys

targets:
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
universe_prologue:
	@echo "--------------------------------------------------------------"
	@echo ">>> make universe started on ${STARTTIME}"
	@echo "--------------------------------------------------------------"
.if defined(DOING_TINDERBOX)
	@rm -f ${FAILFILE}
.endif
.for target in ${TARGETS}
universe: universe_${target}
universe_epilogue: universe_${target}
universe_${target}: universe_${target}_prologue
universe_${target}_prologue: universe_prologue
	@echo ">> ${target} started on `LC_ALL=C date`"
.if !defined(MAKE_JUST_KERNELS)
.for target_arch in ${TARGET_ARCHES_${target}}
universe_${target}: universe_${target}_${target_arch}
universe_${target}_${target_arch}: universe_${target}_prologue
	@echo ">> ${target}.${target_arch} ${UNIVERSE_TARGET} started on `LC_ALL=C date`"
	@(cd ${.CURDIR} && env __MAKE_CONF=/dev/null \
	    ${SUB_MAKE} ${JFLAG} ${UNIVERSE_TARGET} \
	    TARGET=${target} \
	    TARGET_ARCH=${target_arch} \
	    > _.${target}.${target_arch}.${UNIVERSE_TARGET} 2>&1 || \
	    (echo "${target}.${target_arch} ${UNIVERSE_TARGET} failed," \
	    "check _.${target}.${target_arch}.${UNIVERSE_TARGET} for details" | \
	    ${MAKEFAIL}))
	@echo ">> ${target}.${target_arch} ${UNIVERSE_TARGET} completed on `LC_ALL=C date`"
.endfor
.endif
.if !defined(MAKE_JUST_WORLDS)
# If we are building world and kernels wait for the required worlds to finish
.if !defined(MAKE_JUST_KERNELS)
.for target_arch in ${TARGET_ARCHES_${target}}
universe_${target}_kernels: universe_${target}_${target_arch}
.endfor
.endif
universe_${target}: universe_${target}_kernels
universe_${target}_kernels: universe_${target}_prologue
.if exists(${KERNSRCDIR}/${target}/conf/NOTES)
	@(cd ${KERNSRCDIR}/${target}/conf && env __MAKE_CONF=/dev/null \
	    ${SUB_MAKE} LINT > ${.CURDIR}/_.${target}.makeLINT 2>&1 || \
	    (echo "${target} 'make LINT' failed," \
	    "check _.${target}.makeLINT for details"| ${MAKEFAIL}))
.endif
	@cd ${.CURDIR} && ${SUB_MAKE} ${.MAKEFLAGS} TARGET=${target} \
	    universe_kernels
.endif
	@echo ">> ${target} completed on `LC_ALL=C date`"
.endfor
universe_kernels: universe_kernconfs
.if !defined(TARGET)
TARGET!=	uname -m
.endif
KERNCONFS!=	cd ${KERNSRCDIR}/${TARGET}/conf && \
		find [A-Z0-9]*[A-Z0-9] -type f -maxdepth 0 \
		! -name DEFAULTS ! -name NOTES
universe_kernconfs:
.for kernel in ${KERNCONFS}
TARGET_ARCH_${kernel}!=	cd ${KERNSRCDIR}/${TARGET}/conf && \
	config -m ${KERNSRCDIR}/${TARGET}/conf/${kernel} 2> /dev/null | \
	grep -v WARNING: | cut -f 2
.if empty(TARGET_ARCH_${kernel})
.error "Target architecture for ${TARGET}/conf/${kernel} unknown.  config(8) likely too old."
.endif
universe_kernconfs: universe_kernconf_${TARGET}_${kernel}
universe_kernconf_${TARGET}_${kernel}:
	@(cd ${.CURDIR} && env __MAKE_CONF=/dev/null \
	    ${SUB_MAKE} ${JFLAG} buildkernel \
	    TARGET=${TARGET} \
	    TARGET_ARCH=${TARGET_ARCH_${kernel}} \
	    KERNCONF=${kernel} \
	    > _.${TARGET}.${kernel} 2>&1 || \
	    (echo "${TARGET} ${kernel} kernel failed," \
	    "check _.${TARGET}.${kernel} for details"| ${MAKEFAIL}))
.endfor
universe: universe_epilogue
universe_epilogue:
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

buildLINT:
	${MAKE} -C ${.CURDIR}/sys/${_TARGET}/conf LINT
