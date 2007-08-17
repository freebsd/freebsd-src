# $FreeBSD$
#
# make release [BUILDNAME=somename] CHROOTDIR=/some/dir CVSROOT=/cvs/dir \
#     [RELEASETAG=tag]
#
# Where "/some/dir" is the pathname of a directory on a some filesystem with
# at least 1000MB of free space, "somename" is what you want the release to
# call itself, "/cvs/dir" is where our source repo resides and, optionally,
# which CVS "tag" name should be used when checking out the sources to build
# the release (default is HEAD).
#
# Please note: the md(4) driver must be present in the kernel
# (either by being compiled in or available as a kld(4) module),
# otherwise the target 'release.8' and possibly others will fail.
#
# Note: If you add options to this file, please keep release(7) updated!
#
# Set these, release builder!
#
# Fixed version:
#BUILDNAME=7.0-CURRENT
#
# Automatic SNAP versioning:
DATE != date +%Y%m%d
BASE = 7.0
BUILDNAME?=${BASE}-${DATE}-SNAP
#
#CHROOTDIR=/junk/release
# If this is a -stable snapshot, then set
#RELEASETAG=RELENG_6
#
# To test a release with a source tree containing patches and
# other work. This tree will get copied instead of getting the
# src/ tree from a CVS checkout. For "rerelease", this will NOT
# be copied; cvs update will be used instead.
#EXTSRCDIR=/usr/src
#
# To use a checked-out ports collection directory instead of
# checking out from a local CVS repository, set this option.
#EXTPORTSDIR=/usr/ports
#
# To add other options to the CVS subcommands (co,up), set
#CVSCMDARGS="-D '01/01/2002 00:00:00 UTC'"
#
# To add other options to the CVS command, set
#CVSARGS="-lfq"
#
# To prefix the cvs command
#CVSPREFIX="/usr/bin/time"
#
# Where the CVS repository is
#CVSROOT="/home/ncvs"
#
# Non-zero if ${RELEASETAG} is in the form "RELENG_ver_RELEASE"; we
# are building an official release.  Otherwise, we are building for
# a branch.
.if defined(RELEASETAG)
ISRELEASE!=	expr "${RELEASETAG}" : '^RELENG_.*_RELEASE$$' || true
.if ${ISRELEASE} != 0
# Convert "RELENG_ver_RELEASE" to "RELEASE_ver" for ports and doc trees.
AUXRELEASETAG!=	echo ${RELEASETAG} | sed -e 's/^RELENG_/RELEASE_/' -e 's/_RELEASE$$//'
DOCRELEASETAG?=		${AUXRELEASETAG}
PORTSRELEASETAG?=	${AUXRELEASETAG}
.endif
.endif

# If you want to pass flags to the world build such as -j X, use
# WORLD_FLAGS.  Similarly, you can specify make flags for kernel
# builds via KERNEL_FLAGS.
#WORLD_FLAGS=-j4
#KERNEL_FLAGS=-j4

TARGET_ARCH?=	${MACHINE_ARCH}
.if ${TARGET_ARCH} == ${MACHINE_ARCH}
TARGET?=	${MACHINE}
.else
TARGET?=	${TARGET_ARCH}
.endif
CROSSENV=	TARGET_ARCH=${TARGET_ARCH} TARGET=${TARGET}
CROSSMAKE=	${MAKE} ${CROSSENV}
NATIVEMAKE=	${MAKE} TARGET_ARCH=${MACHINE_ARCH} TARGET=${MACHINE}

# If you are using a local CVS repository with components stored in 
# non-standard modules, override these on the make commandline or
# in the environment.
RELEASESRCMODULE?=	src
RELEASEDOCMODULE?=	doc
RELEASEPORTSMODULE?=	ports

# Uncomment this to disable the doc.1 target.  Docs normally require
# the ports tree, so NOPORTS can be set together with NODOC in order
# to have neither ports or docs.  If only NOPORTS is set to YES, but
# docs are still desired, the DOMINIMALDOCPORTS logic below will only
# install the ports that are minimally required for the docs.  This is
# intended as a compromise, less disk space is required than for using
# the entire ports collection (and much less time due to the huge number
# of directories it would create), but still quite a bit as well as some
# CPU cycles (some of the programs are C++, and things like ghostscript
# belong to the required ports nevertheless).
#
# Setting this also disables building of release note documentation
# (RELNOTESng).
#NODOC=  YES
#NOPORTS=  YES

# When retrieving ports using the pkg_add -r method, set your proxies to these.
# src/release/${arch}/mkisoimages.sh can use these.
#HTTP_PROXY?=	www.example.com
#FTP_PROXY?=	ftp.example.com

# When creating ISO images, point ${CD_PACKAGE_TREE} to a directory containing
# the package split by an earlier invocation of the 'package-split' target.
#CD_PACKAGE_TREE=	/path/to/pkg

# Extra source tarballs; each argument is a pair of source dir and
# distribution name.  The dist name should not exceed 7 characters
# (another "s" for "source" will be prepended).
#EXTRA_SRC=	games/fortune fortune

# Modify this definition if you want the release notes 
# and other release documentation in a language other than English.
RELNOTES_LANG?=	en_US.ISO8859-1

# As an alternative to installing the entire ports collection (which
# can take a huge amount of time, in particular on slower disks),
# setting ${MINIMALDOCPORTS} allows to install and build just those
# ports that are really required for getting the docs up & running.
.if defined(NOPORTS) && !defined(NODOC)
DOMINIMALDOCPORTS=	YES
.include "Makefile.inc.docports"
RELEASEPORTSMODULE=	${MINIMALDOCPORTS}
.endif

# Helper variable
.if defined(NOPORTS)
.if !defined(DOMINIMALDOCPORTS) || ${DOMINIMALDOCPORTS} != "YES"
NOPORTSATALL=	YES
.endif
.endif

#
# Doing 'make index' in /usr/ports requires Perl.
MAKEINDEXPORTS=	lang/perl5.8
# By default, documentation (Handbook, FAQ, etc.) is built for all
# the languages.  To speed up building, set the DOC_LANG to just
# the languages you need.  (The language for the release notes is
# controlled by the RELNOTES_LANG variable above.)
#DOC_LANG=	en_US.ISO8859-1
DOCPORTS=	textproc/docproj
# Set this to wherever the distfiles required by release procedures.
.if defined(DOCDISTFILES)
# Respect DOCDISTFILES which is used before.
RELEASEDISTFILES?=	${DOCDISTFILES}
.else
RELEASEDISTFILES?=	${.CURDIR}/../../ports/distfiles
.endif
# Set this to 1 if you want -P to be used for automatic keyboard detection
# on the boot floppy.  WARNING: Breaks on some Athlon (K7) motherboards.
AUTO_KEYBOARD_DETECT?= 0

.if !defined(NODOC)
DIST_DOCS_ARCH_INDEP=	hardware readme relnotes errata
DIST_DOCS_ARCH_DEP=	
.endif

# Things which without too much trouble can be considered variables
# BASE_DISTS are special in that they get full /etc installation sets.
#
OTHER_DISTS?=	catpages manpages games proflibs dict info doc ${ARCH_DISTS}
BASE_DISTS?=	base
.if ${TARGET_ARCH} == "amd64"
ARCH_DISTS?=	lib32
.endif
DISTRIBUTIONS?=	${BASE_DISTS} ${OTHER_DISTS}

#
# Build and package both GENERIC and SMP kernels if the target
# has both configuration files.  Otherwise only GENERIC is done.
#
.if exists(${.CURDIR}/../sys/${TARGET}/conf/SMP)
KERNELS_BASE?=	GENERIC SMP
.else
KERNELS_BASE?=	GENERIC
.endif

# mountpoint for filesystems.
MNT=			/mnt

# Various floppy image parameters.
#

.undef MAKE_FLOPPIES
.if ${TARGET_ARCH} == "i386"
MAKE_FLOPPIES=		true
SPLIT_MFSROOT=
.if ${TARGET} == "pc98"
SMALLFLOPPYSIZE=	1200
SMALLFLOPPYSPLITSIZE=	1152
SMALLFLOPPYLABEL=	fd1200
SPLIT_MFSROOT=
.endif
FLOPPYSIZE=		1440
FLOPPYSPLITSIZE=	1392
FLOPPYINODE=		40000
FLOPPYLABEL=		fd1440
BOOTINODE=		80000
MFSSIZE=		4320
MFSINODE=		8000
MFSLABEL=		minimum3
.elif ${TARGET_ARCH} == "sparc64"
DISKLABEL=		sunlabel
MFSSIZE=		4096
MFSINODE=		8192
MFSLABEL=		auto
MINIROOT=
.elif ${TARGET_ARCH} == "ia64"
MFSSIZE=		8192
MFSINODE=		8192
MFSLABEL=		auto
SEPARATE_LIVEFS=
.elif ${TARGET_ARCH} == "amd64"
MAKE_FLOPPIES=		true
FLOPPYSIZE=		1440
FLOPPYSPLITSIZE=	1392
FLOPPYINODE=		40000
FLOPPYLABEL=		fd1440
BOOTINODE=		80000
MFSSIZE=		4096
MFSINODE=		8192
MFSLABEL=		auto
SEPARATE_LIVEFS=
SPLIT_MFSROOT=
.elif ${TARGET_ARCH} == "powerpc"
DISKLABEL=		""
MFSSIZE=		4096
MFSINODE=		8192
MFSLABEL=		auto
SEPARATE_LIVEFS=
.endif
.if defined(NO_FLOPPIES)
.undef MAKE_FLOPPIES
.endif

.if exists(/sbin/bsdlabel)
DISKLABEL?=		bsdlabel
.else
DISKLABEL?=		disklabel
.endif

ZIPNSPLIT=		gzip --no-name -9 -c | split -b 1392k -

# Things which may get you into trouble if you change them
MTREEFILES=		${.CURDIR}/../etc/mtree
_R?=			/R
RD=			${_R}/stage
RND=			${RD}/release.doc
FD=			${_R}/ftp
CD=			${_R}/cdrom
CD_BOOT=		${CD}/bootonly
CD_DISC1=		${CD}/disc1
CD_DISC2=		${CD}/disc2
.if !defined(NODOC)
CD_DOCS=		${CD}/docs
.endif
.if defined(SEPARATE_LIVEFS)
CD_LIVEFS=		${CD}/livefs
.else
CD_LIVEFS=		${CD_DISC1}
.endif
_MK?=			${CHROOTDIR}/mk

# Where the bootstrap ports (see DOCPORTS) get installed.
LOCALDIR=		/usr/local/bin

.if ${TARGET} != ${MACHINE} && ${DISKLABEL} == "bsdlabel"
DOFS_SH=	${.CURDIR}/scripts/doFS.sh ${DISKLABEL} ${TARGET}
.else
DOFS_SH=	${.CURDIR}/scripts/doFS.sh ${DISKLABEL} ""
.endif

CRUNCH_TARGETS=	boot
.if ${TARGET_ARCH} == "i386"
CRUNCH_TARGETS+=fixit
.if ${TARGET} == "pc98"
CRUNCH_TARGETS+=fixit-small
.endif
.endif

.if defined(MAKE_FLOPPIES)
EXTRAS= floppies.1
.if ${TARGET_ARCH} == "i386"
EXTRAS+= floppies.2
.endif
EXTRAS+= floppies.3
.endif
EXTRAS+= ftp.1
.if !defined(NOCDROM)
EXTRAS+= cdrom.1 cdrom.2 cdrom.3
.if defined(MAKE_ISOS)
EXTRAS+= iso.1
.endif
.if ${TARGET} == "pc98"
BOOTABLE="-G"
.else
BOOTABLE="-b"
.endif
.endif

.if !defined(NODOC)
DOCREL= doc.1 doc.2
.endif

.if !make(release) && !make(rerelease) && !make(package-split)
BINMAKE!=	cd ${.CURDIR}/..; ${MAKE} -V BINMAKE
WMAKEENV!=	cd ${.CURDIR}/..; \
		${BINMAKE} ${CROSSENV} -f Makefile.inc1 -V WMAKEENV
WMAKE=		${WMAKEENV} ${BINMAKE}
.endif

CVS_SRCARGS=	-P
.if defined(RELEASETAG)
CVS_SRCARGS+=	-r ${RELEASETAG}
.endif

CVS_DOCARGS=	-P
.if defined(DOCRELEASETAG)
CVS_DOCARGS+=	-r ${DOCRELEASETAG}
.endif

CVS_PORTSARGS=	-P
.if defined(PORTSRELEASETAG)
CVS_PORTSARGS+=	-r ${PORTSRELEASETAG}
.endif

WORLDDIR?=	${.CURDIR}/..

release rerelease:
.if !defined(CHROOTDIR) || !defined(BUILDNAME) || !defined(CVSROOT)
	@echo "To make a release you must set CHROOTDIR, BUILDNAME and CVSROOT" && false
.endif
.if defined(NOPORTSATALL) && !defined(NODOC)
	@echo "Ports are required for building the docs.  Either set NODOC or"
	@echo "unset NOPORTS, or set at least DOMINIMALDOCPORTS to YES!"
	@exit 1
.endif
.if defined(LOCAL_PATCHES) && !empty(LOCAL_PATCHES)
.for p in ${LOCAL_PATCHES}
.if !exists(${p})
	@echo "The patch file ${p} does not exist!"
	@exit 1
.endif
.endfor
.endif
.if defined(LOCAL_SCRIPT) && !exists(${LOCAL_SCRIPT})
	@echo "The local script ${LOCAL_SCRIPT} does not exist!"
	@exit 1
.endif
.if make(release)
.if exists(${CHROOTDIR})
# The first command will fail on a handful of files that have their schg
# flags set.  But it greatly speeds up the next two commands.
	# NB: clear any vestigial devfs mount, just in case
	-umount ${CHROOTDIR}/dev > /dev/null 2>&1
	-rm -rf ${CHROOTDIR} 2>/dev/null
	-chflags -R 0 ${CHROOTDIR}/.
	-rm -rf ${CHROOTDIR}
.endif
	mkdir -p ${CHROOTDIR}
	@echo ">>> make release for ${TARGET} started on `LC_ALL=C TZ=GMT date`"
	cd ${WORLDDIR} && ${NATIVEMAKE} -DWITHOUT_GAMES -DWITHOUT_HTML -DWITHOUT_LIB32 \
	    -DWITHOUT_MAN -DWITHOUT_NLS -DWITHOUT_PROFILE installworld DESTDIR=${CHROOTDIR}
	cd ${WORLDDIR} && ${NATIVEMAKE} distribution DESTDIR=${CHROOTDIR}
	if [ -f /etc/resolv.conf ]; then \
		cp -p /etc/resolv.conf ${CHROOTDIR}/etc; \
	fi
.if defined(EXTLOCALDIR)
	rm -rf ${CHROOTDIR}/usr/local
	cd ${CHROOTDIR}/usr && cp -R -H ${EXTLOCALDIR} local
.endif
	rm -rf ${CHROOTDIR}/usr/src
.if defined(EXTSRCDIR)
	cd ${CHROOTDIR}/usr && \
	    cp -R -H ${EXTSRCDIR} src
.else
	cd ${CHROOTDIR}/usr && \
	    ${CVSPREFIX} cvs -R ${CVSARGS} -d ${CVSROOT} \
	    co ${CVSCMDARGS} ${CVS_SRCARGS} ${RELEASESRCMODULE}
.endif
.if defined(LOCAL_PATCHES) && !empty(LOCAL_PATCHES)
.for p in ${LOCAL_PATCHES}
	patch -d ${CHROOTDIR}/usr/${RELEASESRCMODULE} ${PATCH_FLAGS} < ${p}
.endfor
.endif
.if defined(LOCAL_SCRIPT)
	cd ${CHROOTDIR} && env CHROOTDIR=${CHROOTDIR} BUILDNAME=${BUILDNAME} \
	    RELEASETAG=${RELEASETAG} ${LOCAL_SCRIPT}
.endif
	rm -rf ${CHROOTDIR}/usr/ports
.if !defined(NOPORTSATALL)
.if defined(EXTPORTSDIR)
	cd ${CHROOTDIR}/usr && cp -R -H ${EXTPORTSDIR} ports
	# If there are distfiles downloaded removing them
	rm -rf ports/distfiles/*
.else
	cd ${CHROOTDIR}/usr && ${CVSPREFIX} cvs -R ${CVSARGS} -d ${CVSROOT} \
	    co ${CVSCMDARGS} ${CVS_PORTSARGS} ${RELEASEPORTSMODULE}
.endif
.endif
.if !defined(NODOC)
	rm -rf ${CHROOTDIR}/usr/doc
.if defined(EXTDOCDIR)
	cd ${CHROOTDIR}/usr && cp -R -H ${EXTDOCDIR} doc
.else
	cd ${CHROOTDIR}/usr && ${CVSPREFIX} cvs -R ${CVSARGS} -d ${CVSROOT} \
	    co ${CVSCMDARGS} ${CVS_DOCARGS} ${RELEASEDOCMODULE}
.endif
	if [ -d ${RELEASEDISTFILES}/ ]; then \
		cp -rp ${RELEASEDISTFILES} ${CHROOTDIR}/usr/ports/distfiles; \
	else \
		mkdir -p ${CHROOTDIR}/usr/ports/distfiles; \
	fi
.if !defined(NO_PREFETCHDISTFILES)
	@cd ${.CURDIR} && ${MAKE} fetch-distfiles
.endif
.endif
.endif
.if make(rerelease)
.if !defined(RELEASENOUPDATE) && !defined(EXTSRCDIR)
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr/src && ${CVSPREFIX} cvs -R ${CVSARGS} -q \
	    update ${CVSCMDARGS} -P -d -A
.else
	cd ${CHROOTDIR}/usr/src && ${CVSPREFIX} cvs -R ${CVSARGS} -q \
	    update ${CVSCMDARGS} -P -d -r ${RELEASETAG}
.endif
	rm -f ${CHROOTDIR}/tmp/.world_done 
.if !defined(NOPORTS) && !defined(EXTPORTSDIR)
	cd ${CHROOTDIR}/usr/ports && ${CVSPREFIX} cvs -R ${CVSARGS} -q \
	    update ${CVSCMDARGS} -P -d
	rm -f ${CHROOTDIR}/tmp/.skip_ports_index
.endif
.if defined(DOMINIMALDOCPORTS) && ${DOMINIMALDOCPORTS} == "YES"
	for i in ${MINIMALDOCPORTS}; do \
	    ( cd ${CHROOTDIR}/usr/$$i && ${CVSPREFIX} cvs -R ${CVSARGS} -q \
		update ${CVSCMDARGS} -P -d ) ; \
	done
.endif
.if !defined(NODOC)
	cd ${CHROOTDIR}/usr/doc && ${CVSPREFIX} cvs -R ${CVSARGS} -q \
	    update ${CVSCMDARGS} -P -d
.endif
.endif
.endif
	# Add version information to those things that need it.
	if [ ! -f ${CHROOTDIR}/tmp/.world_done ]; then \
		cd ${CHROOTDIR}/usr/src/sys/conf && \
		mv newvers.sh foo && \
		sed "s/^RELEASE=.*/RELEASE=${BUILDNAME}/" foo > newvers.sh && \
		rm foo; \
	fi
	-test -f install.cfg && cp install.cfg ${CHROOTDIR}/usr/src/release
	echo "#!/bin/sh"			> ${_MK}
	echo "set -ex"				>> ${_MK}
	echo "trap 'umount /dev || true' 0"	>> ${_MK}
	echo "_RELTARGET=\$${1:-doRELEASE}"	>> ${_MK}
.for var in \
	AUTO_KEYBOARD_DETECT \
	BUILDNAME \
	CD_PACKAGE_TREE \
	DISTRIBUTIONS \
	DOC_LANG \
	DOMINIMALDOCPORTS \
	EXTRA_SRC \
	FTP_PASSIVE_MODE \
	FTP_PROXY \
	HTTP_PROXY \
	KERNELS \
	KERNELS_BASE \
	KERNEL_FLAGS \
	MAKE_FLOPPIES \
	MAKE_ISOS \
	NOCDROM \
	NODOC \
	NOPORTS \
	NO_SHARED \
	NOSRC \
	NO_CPU_CFLAGS \
	NO_CPU_COPTFLAGS \
	NO_FLOPPIES \
	RELEASETAG \
	RELNOTES_LANG \
	SEPARATE_LIVEFS \
	TARGET \
	TARGET_ARCH \
	WORLD_FLAGS
.if defined(${var})
	echo "export ${var}=\"${${var}}\""	>> ${_MK}
.endif
.endfor
	# Don't remove this, or the build will fall over!
	echo "export RELEASEDIR=${_R}"		>> ${_MK}
	echo "export PATH=/bin:/usr/bin:/sbin:/usr/sbin:${LOCALDIR}"	>> ${_MK}
	echo "export MANBUILDCAT=YES"		>> ${_MK}
	# NB: these may fail if the host is running w/o devfs
	echo "umount /dev >/dev/null 2>&1 || true" >> ${_MK}
	echo "mount -t devfs devfs /dev >/dev/null 2>&1 || true"	>> ${_MK}
	echo "if [ ! -c /dev/null ]; then" >> ${_MK}
	echo "	echo /dev/null is not a device!" >> ${_MK}
	echo "	exit 1" >> ${_MK}
	echo "fi" >> ${_MK}
	echo "if [ -x /etc/rc.d/ldconfig ]; then" >> ${_MK}
	echo "	/etc/rc.d/ldconfig start"	>> ${_MK}
	echo "else"				>> ${_MK}
	echo "	ldconfig /lib /usr/lib /usr/local/lib || true" >> ${_MK}
	echo "fi"				>> ${_MK}
	echo "if [ ! -f /tmp/.world_done ]; then" >> ${_MK}
	echo "	cd /usr/src"			>> ${_MK}
	echo "	${CROSSMAKE} ${WORLD_FLAGS} -DNO_CLEAN buildworld && \\" >> ${_MK}
	echo "	touch /tmp/.world_done || exit 1" >> ${_MK}
	echo "fi"				>> ${_MK}
	echo "if [ ! -f /tmp/.skip_ports_index ]; then" >> ${_MK}
	echo "	echo \">>> make index started on \`LC_ALL=C TZ=GMT date\`\"" >> ${_MK}
	echo "	for i in ${MAKEINDEXPORTS}"	>> ${_MK}
	echo "	do"			>> ${_MK}
	echo "		cd /usr/ports/\$${i}"	>> ${_MK}
	echo "		env -i FTP_PASSIVE_MODE=$${FTP_PASSIVE_MODE:-no} PATH=$${PATH} \\" >> ${_MK}
	echo "		make all install clean BATCH=yes FORCE_PKG_REGISTER=yes" >> ${_MK}
	echo "	done"				>> ${_MK}
	echo "	cd /usr/ports"			>> ${_MK}
	echo "	rm -f INDEX*"			>> ${_MK}
	echo "	make index -DINDEX_PRISTINE"	>> ${_MK}
	echo "	rm -f INDEX*.tmp"		>> ${_MK}
	echo "	touch /tmp/.skip_ports_index"	>> ${_MK}
	echo "	echo \">>> make index finished on \`LC_ALL=C TZ=GMT date\`\"" >> ${_MK}
	echo "fi"				>> ${_MK}
	echo "cd /usr/src/release"		>> ${_MK}
	echo "make obj"				>> ${_MK}
	echo "make \$${_RELTARGET}"		>> ${_MK}
	echo "echo \">>> make ${.TARGET} for ${TARGET} finished on \`LC_ALL=C TZ=GMT date\`\"" >> ${_MK}
	chmod 755 ${_MK}
.if defined(NOPORTS)
	touch ${CHROOTDIR}/tmp/.skip_ports_index
.endif
	# Ensure md.ko is loaded if md(4) is not statically compiled into 
	# the kernel
	-mdconfig 2>/dev/null
	env -i /usr/sbin/chroot `dirname ${_MK}` /`basename ${_MK}`

clean:
	rm -rf ${CRUNCH_TARGETS:S/$/_crunch/} release.[0-8] ${EXTRAS}

fetch-distfiles:
	@for i in ${MAKEINDEXPORTS}; do \
		cd ${CHROOTDIR}/usr/ports/$$i && \
			make PORTSDIR=${CHROOTDIR}/usr/ports BATCH=yes \
			checksum-recursive ; \
	done
	@for i in ${DOCPORTS}; do \
		cd ${CHROOTDIR}/usr/ports/$$i && \
			make PORTSDIR=${CHROOTDIR}/usr/ports BATCH=yes \
			WITHOUT_X11=yes JADETEX=no WITHOUT_PYTHON=yes \
			checksum-recursive ; \
	done

# Clean out ${_R} and make the directory structure.
release.1:
	mkdir -p ${_R}
	-rm -rf ${_R}/* 2> /dev/null
	-chflags -R noschg ${_R}/.
	rm -rf ${_R}/*
	mkdir ${RD}
.if defined(MAKE_FLOPPIES)
	mkdir ${RD}/floppies
.endif
	mkdir ${RD}/trees
	for i in ${DISTRIBUTIONS}; do \
		mkdir ${RD}/trees/$$i && \
		mtree -deU -f ${MTREEFILES}/BSD.root.dist \
		    -p ${RD}/trees/$$i > /dev/null && \
		mtree -deU -f ${MTREEFILES}/BSD.usr.dist \
		    -p ${RD}/trees/$$i/usr > /dev/null && \
		mtree -deU -f ${MTREEFILES}/BSD.include.dist \
		    -p ${RD}/trees/$$i/usr/include > /dev/null && \
		mtree -deU -f ${MTREEFILES}/BSD.var.dist \
		    -p ${RD}/trees/$$i/var > /dev/null ; \
	done
	mkdir ${RD}/kernels
	for i in ${KERNELS_BASE} ${KERNELS}; do \
		mkdir -p ${RD}/kernels/$${i}; \
	done
	touch ${.TARGET}

# Install the system into the various distributions.
release.2:
	cd ${.CURDIR}/.. && ${CROSSMAKE} distrib-dirs DESTDIR=${RD}/trees/base
	cd ${.CURDIR}/.. && ${CROSSMAKE} ${WORLD_FLAGS} distributeworld \
	    DISTDIR=${RD}/trees
	touch ${.TARGET}

# Make and install the generic kernel(s).
release.3:
.for kernel in ${KERNELS_BASE} ${KERNELS}
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${KERNEL_FLAGS} \
 	    KERNCONF=${kernel} kernel \
 	    DESTDIR=${RD}/kernels KODIR=/${kernel}
.endfor
	touch ${.TARGET}

# Make and install the three crunched binaries which live on the floppies.
# You are not supposed to like this :-)
release.4:
	rm -rf ${RD}/crunch
	mkdir -p ${RD}/crunch
.for j in ${CRUNCH_TARGETS}
.if exists(${.CURDIR}/${TARGET}/${j}_crunch.conf)
	rm -rf ${j}_crunch
	mkdir ${j}_crunch
	cd ${j}_crunch; ${WMAKEENV} MAKEFLAGS="-m ${.CURDIR}/../share/mk" \
	    crunchgen -o \
	    ${.CURDIR}/${TARGET}/${j}_crunch.conf
	cd ${j}_crunch; ${WMAKE} -f ${j}_crunch.mk subclean
	cd ${.CURDIR}/..; ${BINMAKE} -f Makefile.inc1 _build-tools
	cd ${j}_crunch; CFLAGS="-Os -pipe" ${WMAKE} -f ${j}_crunch.mk \
	    -DNO_CPU_CFLAGS all
	${WMAKEENV} strip -R .comment ${j}_crunch/${j}_crunch
	mv ${j}_crunch/${j}_crunch ${RD}/crunch/${j}
.endif
.endfor
	touch ${.TARGET}

#
# --==## Fix up the distributions. ##==--
#
release.5:
	# Create any "synthetic dists" now.
	@for i in ${DISTRIBUTIONS}; do \
		if [ -f ${.CURDIR}/scripts/$${i}-make.sh ]; then \
			echo -n "Running $$i dist creation script... "; \
			env RD=${RD} sh ${.CURDIR}/scripts/$${i}-make.sh || echo "$$i distribution script returned bad status."; \
			echo "Done."; \
		fi \
	done \

	# Remove all the directories we don't need.
	-cd ${RD}/trees && \
		(find ${OTHER_DISTS} -path '*/var/empty' | xargs chflags noschg; \
		find ${OTHER_DISTS} -depth -type d -empty -print | xargs rmdir)
	touch ${.TARGET}

#
# --==## Package up the tarballs from assembled trees ##==--
#
release.6:
	rm -rf ${RD}/dists
	mkdir -p ${RD}/dists
	@for i in ${DISTRIBUTIONS} ; \
	do \
		if [ -d ${RD}/trees/$${i} ] ; then \
			cd ${.CURDIR} && $(MAKE) doTARBALL \
				SD=${RD}/trees/$${i} \
				TN=$$i TD=$$i ARG="." && \
			echo "$${i} distribution is finished."; \
		fi ; \
	done
	@for i in ${KERNELS_BASE} ${KERNELS} ; \
	do \
		if [ -d ${RD}/kernels/$${i} ] ; then \
			cd ${.CURDIR} && $(MAKE) doTARBALL \
				SD=${RD}/kernels \
				TN=$$i TD=kernels ARG="$$i" && \
			echo "$${i} distribution is finished."; \
		fi ; \
	done
.if !defined(NOPORTS)
	# XXX: Inline stripped version of doTARBALL
	@rm -rf ${RD}/dists/ports/ports*
	@mkdir -p ${RD}/dists/ports
	@echo rolling ports/ports tarball
	@tar --exclude CVS --exclude 'ports/distfiles/*' \
	  -czf ${RD}/dists/ports/ports.tgz -C /usr ports
	@cp ${.CURDIR}/scripts/ports-install.sh ${RD}/dists/ports/install.sh
	@(cd ${RD}/dists/ports; \
	  rm -f CHECKSUM.MD5 CHECKSUM.SHA256; \
	  md5 * > .CHECKSUM.MD5; \
	  sha256 * > .CHECKSUM.SHA256; \
	  mv .CHECKSUM.MD5 CHECKSUM.MD5; \
	  mv .CHECKSUM.SHA256 CHECKSUM.SHA256)
	@echo "ports distribution is finished."
.endif
	touch ${.TARGET}


#
# --==## Make source dists ##==--
#
release.7:
.if !defined(NOSRC)
	@cd ${.CURDIR} && $(MAKE) doTARBALL SD=/usr/src \
		TD=src TN=sbase ARG="[A-Z]*"
	@for i in `cd /usr/src && echo [a-z]*` ; do \
		if [ -d /usr/src/$$i ] ; then \
			cd ${.CURDIR} && $(MAKE) doTARBALL \
				TN=`echo s$$i | tr -d '.' | \
				    sed -e 's/usr/u/' \
					-e 's/kerberos5/krb5/'` \
				SD=/usr/src TD=src ARG="$$i" ; \
		fi ; \
	done
.if defined(EXTRA_SRC)
	@set ${EXTRA_SRC} && \
	while [ $$# -ge 2 ] ; do \
		if [ -d /usr/src/$$1 ] ; then \
			cd ${.CURDIR} && $(MAKE) doTARBALL \
				SD=/usr/src TN="s$$2" TD=src ARG="$$1" ; \
		fi && shift && shift ; \
	done
.endif
	(cd ${RD}/dists/src; \
	  rm -f CHECKSUM.MD5 CHECKSUM.SHA256; \
	  md5 * > .CHECKSUM.MD5; \
	  sha256 * > .CHECKSUM.SHA256; \
	  mv .CHECKSUM.MD5 CHECKSUM.MD5; \
	  mv .CHECKSUM.SHA256 CHECKSUM.SHA256)
	@echo "src distribution is finished."
.endif
	touch ${.TARGET}

# Build the memory root filesystem.
release.8:
	cp ${RD}/trees/base/etc/disktab /etc
	rm -rf ${RD}/mfsfd
	mkdir ${RD}/mfsfd
	cd ${RD}/mfsfd && \
		mkdir -p etc/defaults dev mnt stand/etc/defaults stand/help \
		var/empty
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=boot \
		DIR=${RD}/mfsfd/stand ZIP=false
	( cd ${RD}/mfsfd && \
	  for dir in bin sbin ; do \
		ln -sf /stand $$dir; \
	  done )
	cp ${RD}/trees/base/sbin/dhclient-script ${RD}/mfsfd/stand
.if ${TARGET} == "pc98"
	cp ${.CURDIR}/../etc/defaults/pccard.conf \
	    ${RD}/mfsfd/etc/defaults/pccard.conf
.endif
	cp ${.CURDIR}/../etc/master.passwd ${RD}/mfsfd/etc/master.passwd
	cp ${RD}/trees/base/etc/*pwd.db ${RD}/mfsfd/etc/
	( for F in defaults/rc.conf netconfig protocols ; do \
		sed -e '/^#.*$$/d' -e 's/[:space:]*#.*$$//g' \
		${RD}/trees/base/etc/$$F > ${RD}/mfsfd/stand/etc/$$F ; \
	  done )
	grep -E '^(ftp|nameserver|domain|sunrpc|cmd|nfsd)[^-\w]' \
	    ${RD}/trees/base/etc/services | \
	    sed -e '/^#.*$$/d' -e 's/[:space:]*#.*$$//g' \
	    > ${RD}/mfsfd/stand/etc/services
	grep 'operator' ${RD}/trees/base/etc/group \
	    > ${RD}/mfsfd/stand/etc/group
	ln ${RD}/mfsfd/stand/etc/services ${RD}/mfsfd/etc/services
	ln ${RD}/mfsfd/stand/etc/group ${RD}/mfsfd/etc/group
	ln ${RD}/mfsfd/stand/etc/netconfig ${RD}/mfsfd/etc/netconfig
	cp ${RD}/trees/base/COPYRIGHT ${RD}/mfsfd/stand/help/COPYRIGHT.hlp
.if !defined(NODOC)
	@for i in ${DIST_DOCS_ARCH_INDEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.txt \
	      ${RD}/mfsfd/stand/help/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.txt \
	      ${RD}/mfsfd/stand/help/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	done
.endif
	-test -f ${.CURDIR}/install.cfg \
	    && cp ${.CURDIR}/install.cfg ${RD}/mfsfd
	@mkdir -p ${RD}/mfsfd/boot
.if ${TARGET_ARCH} != "ia64" && ${TARGET_ARCH} != "powerpc"
	@cp ${RD}/trees/base/boot/boot* ${RD}/mfsfd/boot
.endif
.if ${TARGET} == "i386" || ${TARGET_ARCH} == "amd64"
	@cp ${RD}/trees/base/boot/mbr ${RD}/mfsfd/boot
.endif
	@tar --exclude CVS -cf - -C ${.CURDIR}/../usr.sbin/sysinstall help | \
		tar xf - -C ${RD}/mfsfd/stand
	@mkdir -p ${RD}/mfsroot
	sh -e ${DOFS_SH} ${RD}/mfsroot/mfsroot ${RD} ${MNT} \
	    ${MFSSIZE} ${RD}/mfsfd ${MFSINODE} ${MFSLABEL}
	@gzip -9fnv ${RD}/mfsroot/mfsroot
	touch ${.TARGET}

KERNFLOPPYSET=		${RD}/floppyset/kern/kernel.gz
.if defined(SMALLFLOPPYSIZE)
SMALLKERNFLOPPYSET=	${RD}/floppyset/kern-small/kernel.gz
.endif
.if defined(SPLIT_MFSROOT)
MFSROOTFLOPPYSET=	${RD}/floppyset/mfsroot/mfsroot.gz
.if defined(SMALLFLOPPYSIZE)
SMALLMFSROOTFLOPPYSET=	${RD}/floppyset/mfsroot-small/mfsroot.gz
.endif
.endif

# Build boot and install floppies.
floppies.1:
	@gzip -9nc ${RD}/kernels/GENERIC/kernel > ${RD}/kernels/kernel.gz
	@echo "Making the kernel boot floppies..."
	@cd ${.CURDIR} && ${MAKE} makeFloppySet FLOPPYBASE=kern \
	    FLOPPYDESC="Kernel" SPLITFILE=${RD}/kernels/kernel.gz
.if defined(SMALLFLOPPYSIZE)
	@echo "Making the small kernel boot floppies..."
	@cd ${.CURDIR} && ${MAKE} makeFloppySet FLOPPYBASE=kern-small \
	    FLOPPYDESC="Kernel" SPLITFILE=${RD}/kernels/kernel.gz \
	    FDSIZE="SMALL"
.endif
.if defined(SPLIT_MFSROOT)
	@echo "Making the mfsroot boot floppies..."
	@cd ${.CURDIR} && ${MAKE} makeFloppySet FLOPPYBASE=mfsroot \
	    FLOPPYDESC="Memory Filesystem" SPLITFILE=${RD}/mfsroot/mfsroot.gz
.if defined(SMALLFLOPPYSIZE)
	@echo "Making the small mfsroot boot floppies..."
	@cd ${.CURDIR} && ${MAKE} makeFloppySet FLOPPYBASE=mfsroot-small \
	    FLOPPYDESC="Memory Filesystem" SPLITFILE=${RD}/mfsroot/mfsroot.gz \
	    FDSIZE="SMALL"
.endif
	@cd ${.CURDIR} && ${MAKE} buildBootFloppy FSIMAGE="boot" \
	    KERNFILE="${KERNFLOPPYSET}.split ${KERNFLOPPYSET}.boot" \
	    MFSROOTFILE="${MFSROOTFLOPPYSET}.split ${MFSROOTFLOPPYSET}.boot"
.if defined(SMALLFLOPPYSIZE)
	@cd ${.CURDIR} && ${MAKE} buildBootFloppy FSIMAGE="boot-small" \
	    KERNFILE="${SMALLKERNFLOPPYSET}.split ${SMALLKERNFLOPPYSET}.boot" \
	    MFSROOTFILE="${SMALLMFSROOTFLOPPYSET}.split ${SMALLMFSROOTFLOPPYSET}.boot" \
	    FDSIZE="SMALL"
.endif
.else	# !SPLIT_MFSROOT
	@cd ${.CURDIR} && ${MAKE} buildBootFloppy FSIMAGE="boot" \
	    KERNFILE="${KERNFLOPPYSET}.split ${KERNFLOPPYSET}.boot" \
	    MFSROOTFILE=${RD}/mfsroot/mfsroot.gz
.if defined(SMALLFLOPPYSIZE)
	@cd ${.CURDIR} && ${MAKE} buildBootFloppy FSIMAGE="boot-small" \
	    KERNFILE="${SMALLKERNFLOPPYSET}.split ${SMALLKERNFLOPPYSET}.boot" \
	    MFSROOTFILE=${RD}/mfsroot/mfsroot.gz \
	    FDSIZE="SMALL"
.endif
.endif	# SPLIT_MFSROOT
	touch ${.TARGET}

# Build fixit floppy.
floppies.2:
	@echo "Making fixit floppy."
	@rm -rf ${RD}/fixitfd
	@mkdir ${RD}/fixitfd
	@cd ${RD}/fixitfd && \
	    mkdir -p dev stand bin sbin etc mnt mnt1 mnt2 mnt3 mnt4 tmp \
	    usr/share/misc
	@cp ${RD}/trees/base/etc/spwd.db ${RD}/trees/base/etc/group \
	    ${RD}/fixitfd/etc
	@sed -e 's/#.*//' ${RD}/trees/base/etc/protocols \
	    > ${RD}/fixitfd/etc/protocols
	@sed -e 's/#.*//' ${RD}/trees/base/usr/share/misc/scsi_modes \
	    > ${RD}/fixitfd/usr/share/misc/scsi_modes
	@cp ${.CURDIR}/fixit.profile ${RD}/fixitfd/.profile
	@cp ${.CURDIR}/fixit.services ${RD}/fixitfd/etc/services
	@cp ${.CURDIR}/scripts/tar.sh ${RD}/fixitfd/stand/tar
	@chmod 555 ${RD}/fixitfd/stand/tar
.if defined(SMALLFLOPPYSIZE)
	@cd ${.CURDIR} && ${MAKE} installCRUNCH CRUNCH=fixit-small \
	    DIR=${RD}/fixitfd/stand ZIP=false
	@sh -e ${DOFS_SH} ${RD}/floppies/fixit-small.flp ${RD} ${MNT} \
	    ${SMALLFLOPPYSIZE} ${RD}/fixitfd ${FLOPPYINODE} ${SMALLFLOPPYLABEL}
	@rm -rf ${RD}/fixitfd/stand
	@mkdir ${RD}/fixitfd/stand
.endif
	@cd ${.CURDIR} && ${MAKE} installCRUNCH CRUNCH=fixit \
	    DIR=${RD}/fixitfd/stand ZIP=false
	@sh -e ${DOFS_SH} ${RD}/floppies/fixit.flp ${RD} ${MNT} \
	    ${FLOPPYSIZE} ${RD}/fixitfd ${FLOPPYINODE} ${FLOPPYLABEL}
	touch ${.TARGET}

# Do our last minute floppies directory setup
floppies.3:
	@(cd ${RD}/floppies; md5 *.flp > CHECKSUM.MD5)
	@(cd ${RD}/floppies; sha256 *.flp > CHECKSUM.SHA256)
	touch ${.TARGET}

#
# --==## Setup a suitable ftp-area ##==--
#
ftp.1:
	@echo "Setting up FTP distribution area"
	@mkdir -p ${FD}
	-@ln -s . ${FD}/${BUILDNAME}
.if defined(MAKE_FLOPPIES)
	@cd ${RD} && find floppies -print | cpio -dumpl ${FD}
.endif
	@cd ${RD}/dists && find . -print | cpio -dumpl ${FD}
.if !defined(NODOC)
	@for i in ${DIST_DOCS_ARCH_INDEP}; do \
		cp ${RND}/${RELNOTES_LANG}/$$i/article.txt \
		    ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
		cp ${RND}/${RELNOTES_LANG}/$$i/article.html \
		    ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
		cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.txt \
		    ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
		cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.html \
		    ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@cp ${RND}/${RELNOTES_LANG}/readme/docbook.css ${FD}
.endif
	@echo "CD_VERSION = ${BUILDNAME}" > ${FD}/cdrom.inf
	touch ${.TARGET}

# Build a live filesystem cdrom image
cdrom.1:
	@echo "Building CDROM live filesystem image"
	@mkdir -p ${CD_LIVEFS}
	@for i in ${DISTRIBUTIONS} ; \
	do \
		if [ -d ${RD}/trees/$${i} ] ; then \
			chflags -R noschg ${RD}/trees/$${i} || true ; \
			( cd ${RD}/trees/$${i} && \
			    find . -depth -print | cpio -dumpl ${CD_LIVEFS} ) ; \
		fi \
	done
	@echo "Copy GENERIC kernel to boot area"
	@cp -Rp ${RD}/kernels/GENERIC/ ${CD_LIVEFS}/boot/kernel
	@rm -f ${CD_LIVEFS}/.profile
	@cp ${.CURDIR}/fixit.profile ${CD_LIVEFS}/.profile
	@ln -sf /rescue ${CD_LIVEFS}/stand
	@echo "Setting up CDROM boot area"
	@rm -f ${CD_LIVEFS}/boot/loader.conf
	@cp ${RD}/mfsroot/mfsroot.gz ${CD_LIVEFS}/boot/mfsroot.gz
	@echo 'mfsroot_load="YES"' > ${CD_LIVEFS}/boot/loader.conf
	@echo 'mfsroot_type="mfs_root"' >> ${CD_LIVEFS}/boot/loader.conf
	@echo 'mfsroot_name="/boot/mfsroot"' >> ${CD_LIVEFS}/boot/loader.conf
.if exists(${RD}/trees/base/boot/device.hints)
	# Break the link to device.hints so we can modify it
	@rm -f ${CD_LIVEFS}/boot/device.hints
	@cp ${RD}/trees/base/boot/device.hints ${CD_LIVEFS}/boot/device.hints
.endif
	@echo "CD_VERSION = ${BUILDNAME}" > ${CD_LIVEFS}/cdrom.inf
	touch ${.TARGET}

# Build disc1 and disc2 cdrom images
cdrom.2:
	@echo "Building CDROM disc1 filesystem image"
	@mkdir -p ${CD_DISC1}/${BUILDNAME}
.if defined(MAKE_FLOPPIES)
	@cd ${RD} && find floppies -print | cpio -dumpl ${CD_DISC1}
.endif
	@cd ${RD}/dists && find . -print | cpio -dumpl ${CD_DISC1}/${BUILDNAME}
.if !defined(NODOC)
	@for i in ${DIST_DOCS_ARCH_INDEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.txt \
	      ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.html \
	      ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.txt \
	      ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.html \
	      ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@cp ${RND}/${RELNOTES_LANG}/readme/docbook.css ${CD_DISC1}
.endif
.if defined(SEPARATE_LIVEFS)
	@cp -Rp ${CD_LIVEFS}/boot ${CD_DISC1}
	@echo "CD_VERSION = ${BUILDNAME}" > ${CD_DISC1}/cdrom.inf
.endif
	@echo "CD_VOLUME = 1" >> ${CD_DISC1}/cdrom.inf
	@echo "Building CDROM disc2 filesystem image"
	@mkdir -p ${CD_DISC2}
	@echo "CD_VERSION = ${BUILDNAME}" > ${CD_DISC2}/cdrom.inf
	@echo "CD_VOLUME = 2" >> ${CD_DISC2}/cdrom.inf
.if !defined(NODOC)
	echo "Building CDROM docs filesystem image"
	@mkdir -p ${CD_DOCS}
	@echo "CD_VERSION = ${BUILDNAME}" > ${CD_DOCS}/cdrom.inf
	@mkdir -p ${CD_DOCS}/usr/share/doc
	@for i in `ls ${CD_LIVEFS}/usr/share/doc`; do \
		if [ -L ${CD_LIVEFS}/usr/share/doc/$$i -o \
		    -d /usr/doc/$$i ]; then \
			mv ${CD_LIVEFS}/usr/share/doc/$$i \
			    ${CD_DOCS}/usr/share/doc; \
		fi \
	done
.endif
	touch ${.TARGET}

#
# --==## Setup a suitable cdrom-area ##==--
#
cdrom.3:
.if defined(CD_BOOT)
	@echo "Building bootonly CDROM filesystem image"
	@mkdir -p ${CD_BOOT}
	@cp -Rp ${CD_LIVEFS}/boot ${CD_BOOT}
	@echo "CD_VERSION = ${BUILDNAME}" > ${CD_BOOT}/cdrom.inf
.if defined(MINIROOT)
	@echo "Building bootonly UFS filesystem image"
	@mkdir -p ${FD}/miniroot
	@sh -e ${DOFS_SH} ${FD}/miniroot/miniroot.ufs \
	    ${RD} ${MNT} 0 ${CD_BOOT} 8192 auto
	@gzip -9v ${FD}/miniroot/miniroot.ufs
.endif
.endif
	touch ${.TARGET}

.if make(iso.1)
.if defined(CD_PACKAGE_TREE)
.if exists(${CD_PACKAGE_TREE}/disc1)
CD_DISC1_PKGS=	${CD_PACKAGE_TREE}/disc1
.endif
.if exists(${CD_PACKAGE_TREE}/disc2)
CD_DISC2_PKGS=	${CD_PACKAGE_TREE}/disc2
.endif
.endif
.endif

iso.1:
.if exists(${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh)
	@echo "Creating ISO images..."
.if defined(CD_BOOT)
	@sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh ${BOOTABLE} \
	    FreeBSD_bootonly \
	    ${CD}/${BUILDNAME}-${TARGET}-bootonly.iso ${CD_BOOT}
.endif
	@sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh ${BOOTABLE} \
	    FreeBSD_Install \
	    ${CD}/${BUILDNAME}-${TARGET}-disc1.iso ${CD_DISC1} \
	    ${CD_DISC1_PKGS}
	@sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh \
	    FreeBSD_Packages \
	    ${CD}/${BUILDNAME}-${TARGET}-disc2.iso ${CD_DISC2} \
	    ${CD_DISC2_PKGS}
.if !defined(NODOC)
	@sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh \
	    FreeBSD_Documentation \
	    ${CD}/${BUILDNAME}-${TARGET}-docs.iso ${CD_DOCS}
.endif
.if defined(SEPARATE_LIVEFS)
	@sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh ${BOOTABLE} \
	    FreeBSD_LiveFS \
	    ${CD}/${BUILDNAME}-${TARGET}-livefs.iso ${CD_LIVEFS}
.endif
	@echo "Generating MD5 and SHA256 sums..."
	@(cd ${CD} && md5 *.iso > ${BUILDNAME}-${TARGET}-iso.CHECKSUM.MD5)
	@(cd ${CD} && sha256 *.iso > ${BUILDNAME}-${TARGET}-iso.CHECKSUM.SHA256)
	touch ${.TARGET}
.else
	@echo "Do not know how to create an ISO for ${TARGET_ARCH}."
.endif

#
# --==## Documentation Project files such as the Handbook and FAQ ##==--
#
doc.1:
	@echo "Making docs..."
	@for i in ${DOCPORTS}; do \
	    cd /usr/ports/$$i && \
	    env -i FTP_PASSIVE_MODE=$${FTP_PASSIVE_MODE:-no} PATH=$${PATH} \
		make all install clean BATCH=yes WITHOUT_X11=yes JADETEX=no \
		WITHOUT_PYTHON=yes FORCE_PKG_REGISTER=yes; \
	done
	@cd /usr/doc && make all install 'FORMATS=html html-split txt' \
	    INSTALL_COMPRESSED='' DOCDIR=${RD}/trees/base/usr/share/doc \
	    URLS_ABSOLUTE=YES	
	touch ${.TARGET}

#
# --==## RELNOTESng:  Next-generation replacements for *.TXT files ##==--
#
doc.2:
	@echo "Making release documentation..."
	@cd ${.CURDIR}/doc && make all install clean 'FORMATS=html txt' \
	    INSTALL_COMPRESSED='' URLS_ABSOLUTE=YES DOCDIR=${RND}
	touch ${.TARGET}

# Various "subroutine" and other supporting targets.

# RD=
# SD=
# TD=
# ARG=
doTARBALL:
.if !defined(SD)
	@echo "SD undefined in doTARBALL" && exit 1
.endif
.if !defined(TD)
	@echo "TD undefined in doTARBALL" && exit 1
.endif
.if !defined(ARG)
	@echo "ARG undefined in doTARBALL" && exit 1
.endif
	@rm -rf ${RD}/dists/${TD}/${TN}*
	@mkdir -p ${RD}/dists/${TD}
	@( cd ${SD} && \
		tn=`echo ${TN} | tr 'A-Z' 'a-z'` && \
		echo rolling ${TD}/$$tn tarball &&\
		tar --exclude CVS --exclude obj --exclude BOOTMFS -cf - ${ARG} | \
		${ZIPNSPLIT} ${RD}/dists/${TD}/$$tn. && \
		sh ${.CURDIR}/scripts/info.sh ${RD}/dists/${TD}/$$tn \
		    > ${RD}/dists/${TD}/$$tn.inf && \
		if [ -f ${.CURDIR}/scripts/$${TD}-install.sh ]; then \
			cp -p ${.CURDIR}/scripts/$${TD}-install.sh \
			    ${RD}/dists/${TD}/install.sh && \
			chmod +x ${RD}/dists/${TD}/install.sh; \
		fi && \
		if [ "${SD}" != "/usr/src" ]; then \
			mtree -c -i -p ${SD}/${ARG} \
			  -k gname,md5digest,mode,nlink,uname,size,link,type \
			  > ${RD}/dists/${TD}/$$tn.mtree ; \
		else \
			true; \
		fi; \
		( cd ${RD}/dists/${TD}; \
		rm -f CHECKSUM.MD5 CHECKSUM.SHA256; \
		md5 * > .CHECKSUM.MD5; \
		sha256 * > .CHECKSUM.SHA256; \
		mv .CHECKSUM.MD5 CHECKSUM.MD5; \
		mv .CHECKSUM.SHA256 CHECKSUM.SHA256) \
	)

doRELEASE: release.1 release.2 ${DOCREL} release.3 release.4 \
    release.5 release.6 release.7 release.8 ${EXTRAS}
	@echo "Release done"

floppies:
	@rm -f release.4 release.8 floppies.[123]
	@cd ${.CURDIR} && ${MAKE} release.4 release.8 floppies.1 floppies.2 \
	    floppies.3
	@cd ${RD} && find floppies -print | cpio -dumpl ${FD}

installCRUNCH:
.if !defined(CRUNCH)
	@echo "CRUNCH undefined in installCRUNCH" && exit 1
.endif
.if !defined(DIR)
	@echo "DIR undefined in installCRUNCH" && exit 1
.endif
.if !defined(ZIP)
	@echo "ZIP undefined in installCRUNCH" && exit 1
.endif
	@if ${ZIP} ; then \
		gzip -9 < ${RD}/crunch/${CRUNCH} > ${DIR}/${CRUNCH}_crunch ; \
	else \
		ln -f ${RD}/crunch/${CRUNCH} ${DIR}/${CRUNCH}_crunch ; \
	fi
	@chmod 555 ${DIR}/${CRUNCH}_crunch
	@if [ -f ${.CURDIR}/${TARGET}/${CRUNCH}_crunch.conf ] ; then \
		for i in `crunchgen -l ${.CURDIR}/${TARGET}/${CRUNCH}_crunch.conf` ; do \
			ln -f ${DIR}/${CRUNCH}_crunch ${DIR}/$$i ; \
		done \
	else \
		for i in `crunchgen -l ${.CURDIR}/${CRUNCH}_crunch.conf` ; do \
			ln -f ${DIR}/${CRUNCH}_crunch ${DIR}/$$i ; \
		done \
	fi

#
# --==## Build a floppy set for a splitfs file ##==--
#
# FLOPPYBASE - basename of floppy image files
# FLOPPYDESC - description of floppy set
# SPLITFILE - filename of the file to split
# FDSIZE - if specified and "small", small floppy is created

.if make(makeFloppySet)
SPLITDIR=	${RD}/floppyset/${FLOPPYBASE}
.if defined(FDSIZE) && ${FDSIZE} == "SMALL"
FLPSPLITSIZE=	${SMALLFLOPPYSPLITSIZE}
FLPSIZE=	${SMALLFLOPPYSIZE}
FLPLABEL=	${SMALLFLOPPYLABEL}
.else
FLPSPLITSIZE=	${FLOPPYSPLITSIZE}
FLPSIZE=	${FLOPPYSIZE}
FLPLABEL=	${FLOPPYLABEL}
.endif
.endif

makeFloppySet:
.if !defined(FLOPPYBASE)
	@echo "FLOPPYBASE undefined in ${.TARGET}" && exit 1
.endif
.if !defined(FLOPPYDESC)
	@echo "FLOPPYDESC undefined in ${.TARGET}" && exit 1
.endif
.if !defined(SPLITFILE)
	@echo "SPLITFILE undefined in ${.TARGET}" && exit 1
.endif
	sh ${.CURDIR}/scripts/split-file.sh ${SPLITFILE} \
	    ${RD}/floppyset/${FLOPPYBASE} ${FLPSPLITSIZE} "${FLOPPYDESC}"
	( splitfile=${SPLITDIR}/`basename ${SPLITFILE}`.split ; \
	lines=`cat $${splitfile} | wc -l`; \
	lines=$$(($$lines - 1)) ; \
	for line in `jot $$lines`; do \
		file=`head -n $$(($${line} + 1)) $${splitfile} | tail -1 | cut -f 1 -d ' '` ; \
		sh -e ${DOFS_SH} ${RD}/floppies/${FLOPPYBASE}$${line}.flp \
		${RD} ${MNT} ${FLPSIZE} ${SPLITDIR}/$${file} \
		${BOOTINODE} ${FLPLABEL}; \
	done )

#
# --==## Build a boot floppy ##==--
#
# FSIMAGE - base floppy image name
# FDSIZE - if specified and "small", small floppy is created
# KERNFILE - path to kernel split file
# MFSROOTFILE - path to mfsroot split file

.if make(buildBootFloppy)
IMAGEDIR=	${RD}/image.${FSIMAGE}
BOOTDIR=	${RD}/trees/base/boot
HINTSFILE=	${BOOTDIR}/device.hints
ACPI_KO=	${RD}/kernels/GENERIC/acpi.ko
IMAGEFILE=	${RD}/floppies/${FSIMAGE}.flp
.if defined(FDSIZE) && ${FDSIZE} == "SMALL"
FLPSIZE=	${SMALLFLOPPYSIZE}
FLPLABEL=	${SMALLFLOPPYLABEL}
.else
FLPSIZE=	${FLOPPYSIZE}
FLPLABEL=	${FLOPPYLABEL}
.endif
.endif

buildBootFloppy:
.if !defined(FSIMAGE)
	@echo "FSIMAGE undefined in ${.TARGET}" && exit 1
.endif
.if !defined(KERNFILE)
	@echo "KERNFILE undefined in ${.TARGET}" && exit 1
.endif
.if !defined(MFSROOTFILE)
	@echo "MFSROOTFILE undefined in ${.TARGET}" && exit 1
.endif
	@echo "Running ${.TARGET} for ${FSIMAGE}"
	@rm -rf ${IMAGEDIR}
	@mkdir ${IMAGEDIR}
	@echo "Setting up /boot directory for ${FSIMAGE} floppy"
	@mkdir -p ${IMAGEDIR}/boot
.if ${TARGET} == "i386"
	@${WMAKEENV} kgzip -v -l ${RD}/trees/base/usr/lib/kgzldr.o -o \
	    ${IMAGEDIR}/boot/loader ${BOOTDIR}/loader
.else
	@cp ${BOOTDIR}/loader ${IMAGEDIR}/boot
.endif
	@cp -Rp ${BOOTDIR}/*.4th ${BOOTDIR}/defaults ${BOOTDIR}/loader.help \
	    ${BOOTDIR}/loader.rc ${IMAGEDIR}/boot
.if exists(${HINTSFILE})
	@cp ${HINTSFILE} ${IMAGEDIR}/boot/device.hints
	@gzip -9n ${IMAGEDIR}/boot/device.hints
.endif
	@gzip -9n ${IMAGEDIR}/boot/*.4th ${IMAGEDIR}/boot/loader.help \
	    ${IMAGEDIR}/boot/defaults/loader.conf
	@echo 'bootfile="/kernel"' > ${IMAGEDIR}/boot/loader.conf
.if exists(${ACPI_KO})
	@gzip -9nc ${ACPI_KO} > ${IMAGEDIR}/acpi.ko.gz
	@echo 'acpi_load="YES"' >> ${IMAGEDIR}/boot/loader.conf
	@echo 'acpi_name="/acpi.ko"' >> ${IMAGEDIR}/boot/loader.conf
	@echo 'acpi_before="read -p \"Insert boot floppy and press Enter\""' >> ${IMAGEDIR}/boot/loader.conf
.endif
	@echo 'mfsroot_load="YES"' >> ${IMAGEDIR}/boot/loader.conf
	@echo 'mfsroot_type="mfs_root"' >> ${IMAGEDIR}/boot/loader.conf
	@echo 'mfsroot_name="/mfsroot"' >> ${IMAGEDIR}/boot/loader.conf
.if !exists(${ACPI_KO})
	@echo 'mfsroot_before="read -p \"Insert boot floppy and press Enter\""' >> ${IMAGEDIR}/boot/loader.conf
.endif
.if defined(SPLIT_MFSROOT)
	@echo 'mfsroot_after="read -p \"Insert boot floppy and press Enter\""' >> ${IMAGEDIR}/boot/loader.conf
.endif
.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "amd64"
.if ${AUTO_KEYBOARD_DETECT}
	@echo "-P" >> ${IMAGEDIR}/boot.config
.endif
.endif
	@rm -f ${IMAGEFILE}
	@cp ${KERNFILE} ${MFSROOTFILE} ${IMAGEDIR}
	sh -e ${DOFS_SH} ${IMAGEFILE} ${RD} ${MNT} ${FLPSIZE} ${IMAGEDIR} \
	    ${BOOTINODE} ${FLPLABEL}
	@echo "Created ${RD}/floppies/${FSIMAGE}.flp"

.if make(package-split)
# Targets related to making a package split
#
# PKG_COPY instructs the script to copy the actual package files rather than
#     make hard links
# PKG_TREE is the path to the package tree to be split
# PKG_DEST is the path to the destination tree to create the split in
# PKG_VERBOSE asks for verbose output of the layout process
# PKG_INDEX is the path to the INDEX file. By default ${PKG_TREE}/INDEX.

.if defined(PKG_COPY)
PKG_DO_COPY=	cp
.else
PKG_DO_COPY=	ln
.endif
PKG_WRKIDX=	${PKG_DEST}/INDEX.master
PKG_ENV?=
.if defined(TARGET_ARCH)
PKG_ENV+=	PKG_ARCH=${TARGET_ARCH}
.endif
.if defined(PKG_VERBOSE)
PKG_ENV+=	PKG_VERBOSE=1
.endif
PKG_INDEX?=	${PKG_TREE}/INDEX

package-split:
.if !defined(PKG_TREE)
	@echo "PKG_TREE must be defined" && exit 1
.endif
.if !defined(PKG_DEST)
	@echo "PKG_DEST must be defined" && exit 1
.endif
	@env ${PKG_ENV} python ${.CURDIR}/scripts/package-split.py \
	    ${PKG_INDEX} ${PKG_WRKIDX}
	@env ${PKG_ENV} sh ${.CURDIR}/scripts/package-trees.sh ${PKG_DO_COPY} \
	    ${PKG_WRKIDX} ${PKG_TREE} ${PKG_DEST}
.endif

.include <bsd.obj.mk>
