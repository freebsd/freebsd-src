#
# $FreeBSD$
#
# The user-driven targets are:
#
# universe 	      - *Really* build *everything*:  Buildworld and
#			all kernels on all architectures.
# buildworld          - Rebuild *everything*, including glue to help do
#                       upgrades.
# installworld        - Install everything built by "buildworld".
# world               - buildworld + installworld.
# buildkernel         - Rebuild the kernel and the kernel-modules.
# installkernel       - Install the kernel and the kernel-modules.
# reinstallkernel     - Reinstall the kernel and the kernel-modules.
# kernel              - buildkernel + installkernel.
# update              - Convenient way to update your source tree (cvs).
# upgrade             - Upgrade a.out (2.2.x/3.0) system to the new ELF way
# most                - Build user commands, no libraries or include files.
# installmost         - Install user commands, no libraries or include files.
# aout-to-elf         - Upgrade an system from a.out to elf format (see below).
# aout-to-elf-build   - Build everything required to upgrade a system from
#                       a.out to elf format (see below).
# aout-to-elf-install - Install everything built by aout-to-elf-build (see
#                       below).
# move-aout-libs      - Move the a.out libraries into an aout sub-directory
#                       of each elf library sub-directory.
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
# 2.  `make world'
#
# For individuals wanting to upgrade their sources (even if only a
# delta of a few days):
#
# 1.  `cd /usr/src'       (or to the directory containing your source tree).
# 2.  `make buildworld'
# 3.  `make buildkernel KERNCONF=YOUR_KERNEL_HERE'     (default is GENERIC).
# 4.  `make installkernel KERNCONF=YOUR_KERNEL_HERE'   (default is GENERIC).
# 5.  `reboot'        (in single user mode: boot -s from the loader prompt).
# 6.  `mergemaster -p'
# 7.  `make installworld'
# 8.  `mergemaster'
# 9.  `reboot'
#
# See src/UPDATING `COMMON ITEMS' for more complete information.
#
# If TARGET_ARCH=arch (e.g. ia64, sparc64, ...) is specified you can
# cross build world for other architectures using the buildworld target,
# and once the world is built you can cross build a kernel using the
# buildkernel target.
#
# ----------------------------------------------------------------------------
#
#           Upgrading an i386 system from a.out to elf format
#
#
# The aout->elf transition build is performed by doing a `make upgrade' (or
# `make aout-to-elf') or in two steps by a `make aout-to-elf-build' followed
# by a `make aout-to-elf-install', depending on user preference.
# You need to have at least 320 Mb of free space for the object tree.
#
# The upgrade process checks the installed release. If this is 3.0-CURRENT,
# it is assumed that your kernel contains all the syscalls required by the
# current sources.
#
# The upgrade procedure will stop and ask for confirmation to proceed
# several times. On each occasion, you can type Ctrl-C to abort the
# upgrade.  Optionally, you can also start it with NOCONFIRM=yes and skip
# the confirmation steps.
#
# ----------------------------------------------------------------------------
#
#
# Define the user-driven targets. These are listed here in alphabetical
# order, but that's not important.
#
TGTS=	all all-man buildkernel buildworld checkdpadd clean \
	cleandepend cleandir depend distribute distributeworld everything \
	hierarchy install installcheck installkernel \
	reinstallkernel installmost installworld libraries lint maninstall \
	mk most obj objlink regress rerelease tags update

BITGTS=	files includes
BITGTS:=${BITGTS} ${BITGTS:S/^/build/} ${BITGTS:S/^/install/}

.ORDER: buildworld installworld
.ORDER: buildworld distributeworld
.ORDER: buildkernel installkernel
.ORDER: buildkernel reinstallkernel

PATH=	/sbin:/bin:/usr/sbin:/usr/bin
MAKE=	PATH=${PATH} make -m ${.CURDIR}/share/mk -f Makefile.inc1

#
# Handle the user-driven targets, using the source relative mk files.
#
${TGTS} ${BITGTS}: upgrade_checks
	@cd ${.CURDIR}; \
		${MAKE} ${.TARGET}

# Set a reasonable default
.MAIN:	all

STARTTIME!= LC_ALL=C date
#
# world
#
# Attempt to rebuild and reinstall *everything*, with reasonable chance of
# success, regardless of how old your existing system is.
#
world: upgrade_checks
	@echo "--------------------------------------------------------------"
	@echo ">>> elf make world started on ${STARTTIME}"
	@echo "--------------------------------------------------------------"
.if target(pre-world)
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> Making 'pre-world' target"
	@echo "--------------------------------------------------------------"
	@cd ${.CURDIR}; ${MAKE} pre-world
.endif
	@cd ${.CURDIR}; ${MAKE} buildworld
	@cd ${.CURDIR}; ${MAKE} -B installworld
.if target(post-world)
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> Making 'post-world' target"
	@echo "--------------------------------------------------------------"
	@cd ${.CURDIR}; ${MAKE} post-world
.endif
	@echo
	@echo "--------------------------------------------------------------"
	@printf ">>> elf make world completed on `LC_ALL=C date`\n                       (started ${STARTTIME})\n"
	@echo "--------------------------------------------------------------"

#
# kernel
#
# Short hand for `make buildkernel installkernel'
#
kernel: buildkernel installkernel

#
# Perform a few tests to determine if the installed tools are adequate
# for building the world. These are for older systems (prior to 2.2.5).
#
# From 2.2.5 onwards, the installed tools will pass these upgrade tests,
# so the normal make world is capable of doing what is required to update
# the system to current.
#
upgrade_checks:
	@cd ${.CURDIR}; \
	if ! make -m ${.CURDIR}/share/mk -Dnotdef test >/dev/null 2>&1; then \
		make make; \
	fi
	@cd ${.CURDIR}; \
	if make -V .CURDIR:C/.// 2>&1 >/dev/null | \
	    grep -q "Unknown modifier 'C'"; then \
		make make; \
	fi

#
# A simple test target used as part of the test to see if make supports
# the -m argument.  Also test that make will only evaluate a conditional
# as far as is necessary to determine its value.
#
test:
.if defined(notdef)
.undef notdef
.if defined(notdef) && ${notdef:U}
.endif
.endif

#
# Upgrade the installed make to the current version using the installed
# headers, libraries and build tools. This is required on installed versions
# prior to 2.2.5 in which the installed make doesn't support the -m argument.
#
make:
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Upgrading the installed make"
	@echo "--------------------------------------------------------------"
	@cd ${.CURDIR}/usr.bin/make; \
		make obj && make -D_UPGRADING depend && \
		make -D_UPGRADING all && make install

#
# Define the upgrade targets. These are listed here in alphabetical
# order, but that's not important.
#
UPGRADE=	aout-to-elf aout-to-elf-build aout-to-elf-install \
		move-aout-libs

#
# Handle the upgrade targets, using the source relative mk files.
#

upgrade:	aout-to-elf

${UPGRADE} : upgrade_checks
	@cd ${.CURDIR}; \
		${MAKE} -f Makefile.upgrade -m ${.CURDIR}/share/mk ${.TARGET}


universe:
	@echo "--------------------------------------------------------------"
	@echo ">>> make universe started on ${STARTTIME}"
	@echo "--------------------------------------------------------------"
.for arch in i386 sparc64 alpha ia64 
	@printf ">> ${arch} started on `LC_ALL=C date`\n"
	-cd ${.CURDIR} && make ${JFLAG} buildworld TARGET_ARCH=${arch} \
		__MAKE_CONF=/dev/null \
		> _.${arch}.buildworld 2>&1
	@printf ">> ${arch} buildworld ended on `LC_ALL=C date`\n"
.if exists(${.CURDIR}/sys/${arch}/conf/NOTES)
	-cd ${.CURDIR}/sys/${arch}/conf && make LINT \
		> _.${arch}.makeLINT 2>&1
.endif
	cd ${.CURDIR} && make buildkernels TARGET_ARCH=${arch} JFLAG="${JFLAG}"
	@printf ">> ${arch} ended on `LC_ALL=C date`\n"
.endfor
	-cd ${.CURDIR} && make buildworld TARGET=pc98 TARGET_ARCH=i386 \
		__MAKE_CONF=/dev/null \
		> _.pc98.buildworld 2>&1
	@echo "--------------------------------------------------------------"
	@printf ">>> make universe completed on `LC_ALL=C date`\n                       (started ${STARTTIME})\n"
	@echo "--------------------------------------------------------------"

KERNCONFS !=	echo ${.CURDIR}/sys/${TARGET_ARCH}/conf/[A-Z]*
KERNCONF2 = ${KERNCONFS:T:N*[a-z]*:NCVS:NNOTES}

buildkernels:
.for kernel in ${KERNCONF2}
.if exists(${.CURDIR}/sys/${TARGET_ARCH}/conf/${kernel})
	-cd ${.CURDIR} && make ${JFLAG} buildkernel \
		TARGET_ARCH=${TARGET_ARCH} KERNCONF=${kernel} \
		__MAKE_CONF=/dev/null \
		 > _.${TARGET_ARCH}.${kernel} 2>&1
.endif
.endfor

