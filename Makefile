#
#	$Id: Makefile,v 1.216 1998/09/09 06:07:32 jb Exp $
#
# The user-driven targets are:
#
# buildworld          - Rebuild *everything*, including glue to help do
#                       upgrades.
# installworld        - Install everything built by "buildworld".
# world               - buildworld + installworld.
# update              - Convenient way to update your source tree (cvs).
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
# The user-driven targets (as listed above) are implemented in Makefile.inc0
# and the private targets are in Makefile.inc1. These are kept separate
# to help the bootstrap build from aout to elf format.
#
# For novices wanting to build from current sources, the simple instructions
# are:
#
# 1.  Ensure that your /usr/obj directory has at least 165 Mb of free space.
# 2.  `cd /usr/src'  (or to the directory containing your source tree).
# 3.  `make world'
#
# Be warned, this will update your installed system, except for configuration
# files in the /etc directory. You have to do those manually.
#
# If at first you're a little nervous about having a `make world' update
# your system, a `make buildworld' will build everything in the /usr/obj
# tree without touching your installed system. To be of any further use
# though, a `make installworld' is required.
#
# The `make world' process always follows the installed object format.
# This is set by creating /etc/objformat containing either OBJFORMAT=aout
# or OBJFORMAT=elf. If this file does not exist, the object format defaults
# to aout. This is expected to be changed to elf just prior to the release
# or 3.0. If OBJFORMAT is set as an environment variable or in /etc/make.conf,
# this overrides /etc/objformat.
#
# Unless -DNOAOUT is specified, a `make world' with OBJFORMAT=elf will
# update the legacy support for aout. This includes all libraries, ld.so,
# lkms and boot objects. This part of build should be regarded as
# deprecated and you should _not_ expect to be able to do this past the
# release of 3.1. You have exactly one major release to move entirely
# to elf.
#
# ----------------------------------------------------------------------------
#
#           Upgrading an i386 system from a.out to elf format
#
#
# The aout->elf transition build is performed by doing a `make aout-to-elf'
# or a `make aout-to-elf-build' followed by a `make aout-to-elf-install'.
# You need to have at least 320 Mb of free space for the object tree.
#
# The upgrade process checks the installed release. If this is 3.0-CURRENT,
# it is assumed that your kernel contains all the syscalls required by the
# current sources.
#
# For installed systems where `uname -r' reports something other than
# 3.0-CURRENT, the upgrade process expects to build a kernel using the
# kernel configuration file sys/i386/conf/GENERICupgrade. This file is
# defaulted to the GENERIC kernel configuration file on the assumption that
# it will be suitable for most systems. Before performing the upgrade,
# replace sys/i386/conf/GENERICupgrade with your own version if your
# hardware requires a different configuration.
#
# The upgrade procedure will stop and ask for confirmation to proceed
# several times. On each occasion, you can type Ctrl-C to abort the
# upgrade.
#
# At the end of the upgrade procedure, /etc/objformat is created or
# updated to contain OBJFORMAT=elf. From then on, you're elf by default.
#
# ----------------------------------------------------------------------------
#
#
# Define the user-driven targets. These are listed here in alphabetical
# order, but that's not important.
#
TGTS =	afterdistribute all buildworld clean cleandepend cleanobj depend \
	distribute everything hierarchy includes installmost install \
	installworld most obj rerelease update world

#
# Handle the user-driven targets, using the source relative mk files.
#
${TGTS} : upgrade_checks
	@cd ${.CURDIR}; \
		make -f Makefile.inc0 -m ${.CURDIR}/share/mk ${.TARGET}

#
# Perform a few tests to determine if the installed tools are adequate
# for building the world. These are for older systems (prior to 2.2.5).
#
# From 2.2.5 onwards, the installed tools will pass these upgrade tests,
# so the normal make world is capable of doing what is required to update
# the system to current.
#
upgrade_checks :
	@cd ${.CURDIR}; if `make -m ${.CURDIR}/share/mk test > /dev/null 2>&1`; then ok=1; else make -f Makefile.upgrade make; fi;

#
# A simple test target used as part of the test to see if make supports
# the -m argument.
#
test	:

#
# Define the upgrade targets. These are listed here in alphabetical
# order, but that's not important.
#
UPGRADE =	aout-to-elf aout-to-elf-build aout-to-elf-install \
		move-aout-libs

#
# Handle the upgrade targets, using the source relative mk files.
#
${UPGRADE} : upgrade_checks
	@cd ${.CURDIR}; \
		make -f Makefile.upgrade -m ${.CURDIR}/share/mk ${.TARGET}
