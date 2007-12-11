#
# $FreeBSD$
#
# The user-driven targets are:
#
# universe            - *Really* build *everything* (buildworld and
#                       all kernels on all architectures).
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
# doxygen             - Build API documentation of the kernel, needs doxygen.
# update              - Convenient way to update your source tree (cvs).
# check-old           - List obsolete directories/files/libraries.
# check-old-dirs      - List obsolete directories.
# check-old-files     - List obsolete files.
# check-old-libs      - List obsolete libraries.
# delete-old          - Delete obsolete directories/files/libraries.
# delete-old-dirs     - Delete obsolete directories.
# delete-old-files    - Delete obsolete files.
# delete-old-libs     - Delete obsolete libraries.
#
# This makefile is simple by design. The FreeBSD make automatically reads
# the /usr/share/mk/sys.mk unless the -m argument is specified on the
# command line. By keeping this makefile simple, it doesn't matter too
# much how different the installed mk files are from those in the source
# tree. This makefile executes a child make process, forcing it to use
# the mk files from the source tree which are supposed to DTRT.
#
# The user-driven targets (as listed above) are implemented in Makefile.inc1.
#
# If you want to build your system from source be sure that /usr/obj has
# at least 400MB of diskspace available.
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
#  8.  `make delete-old'
#  9.  `mergemaster'
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
TGTS=	all all-man buildenv buildenvvars buildkernel buildworld \
	check-old check-old-dirs check-old-files check-old-libs \
	checkdpadd clean cleandepend cleandir \
	delete-old delete-old-dirs delete-old-files delete-old-libs \
	depend distribute distributeworld distrib-dirs distribution doxygen \
	everything hierarchy install installcheck installkernel \
	installkernel.debug reinstallkernel reinstallkernel.debug \
	installworld kernel-toolchain libraries lint maninstall \
	obj objlink regress rerelease showconfig tags toolchain update \
	_worldtmp _legacy _bootstrap-tools _cleanobj _obj \
	_build-tools _cross-tools _includes _libraries _depend \
	build32 distribute32 install32
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
MAKEPATH=	${MAKEOBJDIRPREFIX}${.CURDIR}/make.${MACHINE}
BINMAKE= \
	`if [ -x ${MAKEPATH}/make ]; then echo ${MAKEPATH}/make; else echo ${MAKE}; fi` \
	-m ${.CURDIR}/share/mk
_MAKE=	PATH=${PATH} ${BINMAKE} -f Makefile.inc1

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
# files with chflags set, so this unsets them and tries the 'rm' a
# second time.  There are situations where this target will be cleaning
# some directories via more than one method, but that duplication is
# needed to correctly handle all the possible situations.
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

${TGTS}:
	${_+_}@cd ${.CURDIR}; \
		${_MAKE} ${.TARGET}

# Set a reasonable default
.MAIN:	all

STARTTIME!= LC_ALL=C date

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
upgrade_checks:
	@if ! (cd ${.CURDIR}/tools/build/make_check && \
	    PATH=${PATH} ${BINMAKE} obj >/dev/null 2>&1 && \
	    PATH=${PATH} ${BINMAKE} >/dev/null 2>&1); \
	then \
	    (cd ${.CURDIR} && ${BSDMAKE} make); \
	fi

#
# Upgrade make(1) to the current version using the installed
# headers, libraries and tools.  Also, allow the location of
# the system bsdmake-like utility to be overridden.
#
BSDMAKE?=make
MMAKEENV=	MAKEOBJDIRPREFIX=${MAKEPATH} \
		DESTDIR= \
		INSTALL="sh ${.CURDIR}/tools/install.sh"
MMAKE=		${MMAKEENV} ${BSDMAKE} \
		-D_UPGRADING \
		-DNOMAN -DNO_MAN -DNOSHARED -DNO_SHARED \
		-DNO_CPU_CFLAGS -DNO_WERROR

make: .PHONY
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> Building an up-to-date make(1)"
	@echo "--------------------------------------------------------------"
	${_+_}@cd ${.CURDIR}/usr.bin/make; \
		${MMAKE} obj && \
		${MMAKE} depend && \
		${MMAKE} all && \
		${MMAKE} install DESTDIR=${MAKEPATH} BINDIR=

#
# universe
#
# Attempt to rebuild *everything* for all supported architectures,
# with a reasonable chance of success, regardless of how old your
# existing system is.
#
.if make(universe)
TARGETS?=amd64 arm i386 ia64 pc98 powerpc sparc64 sun4v

universe: universe_prologue
universe_prologue:
	@echo "--------------------------------------------------------------"
	@echo ">>> make universe started on ${STARTTIME}"
	@echo "--------------------------------------------------------------"
.for target in ${TARGETS}
KERNCONFS!=	cd ${.CURDIR}/sys/${target}/conf && \
		find [A-Z]*[A-Z] -type f -maxdepth 0 \
		! -name DEFAULTS ! -name LINT
KERNCONFS:=	${KERNCONFS:S/^NOTES$/LINT/}
universe: universe_${target}
.ORDER: universe_prologue universe_${target} universe_epilogue
universe_${target}:
	@echo ">> ${target} started on `LC_ALL=C date`"
	-cd ${.CURDIR} && ${MAKE} ${JFLAG} buildworld \
	    TARGET=${target} \
	    __MAKE_CONF=/dev/null \
	    > _.${target}.buildworld 2>&1
	@echo ">> ${target} buildworld completed on `LC_ALL=C date`"
.if exists(${.CURDIR}/sys/${target}/conf/NOTES)
	-cd ${.CURDIR}/sys/${target}/conf && ${MAKE} LINT \
	    > ${.CURDIR}/_.${target}.makeLINT 2>&1
.endif
.for kernel in ${KERNCONFS}
	-cd ${.CURDIR} && ${MAKE} ${JFLAG} buildkernel \
	    TARGET=${target} \
	    KERNCONF=${kernel} \
	    __MAKE_CONF=/dev/null \
	    > _.${target}.${kernel} 2>&1
.endfor
	@echo ">> ${target} completed on `LC_ALL=C date`"
.endfor
universe: universe_epilogue
universe_epilogue:
	@echo "--------------------------------------------------------------"
	@echo ">>> make universe completed on `LC_ALL=C date`"
	@echo "                      (started ${STARTTIME})"
	@echo "--------------------------------------------------------------"
.endif
