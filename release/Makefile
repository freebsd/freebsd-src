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
# otherwise the target 'release.9' and possibly others will fail.
#
# Note: If you add options to this file, please keep release(7) updated!
#
# Set these, release builder!
#
# Fixed version:
#BUILDNAME=5.1-RELEASE
#
# Automatic SNAP versioning:
DATE != date +%Y%m%d
BASE = 5.1
BUILDNAME?=${BASE}-${DATE}-SNAP
#
#CHROOTDIR=/junk/release
# If this is a -stable snapshot, then set
#RELEASETAG=RELENG_4
#
# To test a release with a source tree containing patches and
# other work. This tree will get copied instead of getting the
# src/ tree from a CVS checkout. For "rerelease", this will NOT
# be copied; cvs update will be used instead.
#EXTSRCDIR=/usr/src
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
# Similarly, you can specify make flags for make readmes via PORTREADMES_FLAGS.
#WORLD_FLAGS=-j4
#KERNEL_FLAGS=-j4
#PORTREADMES_FLAGS=-j4

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
DIST_DOCS_ARCH_INDEP=	readme errata early-adopter
DIST_DOCS_ARCH_DEP=	installation relnotes hardware
.endif

# Things which without too much trouble can be considered variables
# BASE_DISTS are special in that they get full /etc installation sets.
#
.if ${TARGET_ARCH} == "i386"
COMPAT_DISTS?=	compat1x compat20 compat21 compat22 compat3x compat4x
.elif ${TARGET_ARCH} != "ia64"
COMPAT_DISTS?=	compat4x
.endif
OTHER_DISTS?=	catpages manpages games proflibs dict info doc
CRYPTO_DISTS?=	crypto
BASE_DISTS?=	base
DISTRIBUTIONS?=	${BASE_DISTS} ${OTHER_DISTS} ${COMPAT_DISTS} ${CRYPTO_DISTS}

# mountpoint for filesystems.
MNT=			/mnt

# Various floppy image parameters.
#

.if ${TARGET_ARCH} == "i386"
.if ${TARGET} == "pc98"
SMALLBOOTSIZE=		1200
BOOTSIZE=		1440
SMALLFIXITSIZE=		1200
FIXITSIZE=		1440
MFSSIZE=		4320
BOOTINODE=		80000
FIXITINODE=		40000
MFSINODE=		8000
SMALLBOOTLABEL=		fd1200
BOOTLABEL=		fd1440
SMALLFIXITLABEL=	fd1200
FIXITLABEL=		fd1440
MFSLABEL=		minimum3
.else
DRIVERSIZE=		1440
BOOTSIZE=		1440
FIXITSIZE=		1440
MFSSIZE=		4320
BIGBOOTSIZE=		2880
DRIVERINODE=		40000
BOOTINODE=		80000
FIXITINODE=		40000
MFSINODE=		8000
DRIVERLABEL=		fd1440
BOOTLABEL=		fd1440
FIXITLABEL=		fd1440
MFSLABEL=		minimum3
BIGBOOTLABEL=		minimum2
.endif
.elif ${TARGET_ARCH} == "alpha"
DRIVERSIZE=		1440
BOOTSIZE=		1440
FIXITSIZE=		2880
MFSSIZE=		4320
BIGBOOTSIZE=		2880
DRIVERINODE=		40000
BOOTINODE=		80000
FIXITINODE=		40000
MFSINODE=		8000
DRIVERLABEL=		fd1440
BOOTLABEL=		fd1440
FIXITLABEL=		minimum2
MFSLABEL=		auto
BIGBOOTLABEL=		minimum2
.elif ${TARGET_ARCH} == "sparc64"
DISKLABEL=		sunlabel
BIGBOOTSIZE=		4096
MFSSIZE=		4096
BOOTINODE=		8192
MFSINODE=		8192
BIGBOOTLABEL=		auto
MFSLABEL=		auto
.elif ${TARGET_ARCH} == "ia64"
MFSINODE=		8192
MFSLABEL=		auto
MFSSIZE=		8192
NO_FLOPPIES=
.elif ${TARGET_ARCH} == "amd64"
MFSSIZE=		4096
MFSINODE=		8192
MFSLABEL=		auto
NO_FLOPPIES=
.endif

.if exists(/sbin/bsdlabel)
DISKLABEL?=		bsdlabel
.else
DISKLABEL?=		disklabel
.endif

ZIPNSPLIT=		gzip --no-name -9 -c | split -b 1423k -

# Things that need to be compiled without crypto support in releases
.if !defined(FIXCRYPTO)
FIXCRYPTO!=	cd ${.CURDIR}/../kerberos5; ${MAKE} -V KPROGS; \
		cd ${.CURDIR}/../secure; ${MAKE} -V SPROGS
.endif

# Things which may get you into trouble if you change them
MTREEFILES=		${.CURDIR}/../etc/mtree
_R?=			/R
RD=			${_R}/stage
RND=			${RD}/release.doc
FD=			${_R}/ftp
CD=			${_R}/cdrom
CD_DISC1=		${CD}/disc1
CD_DISC2=		${CD}/disc2

# Where the bootstrap ports (see DOCPORTS) get installed.
LOCALDIR=		/usr/local/bin

.if exists(${.CURDIR}/${TARGET_ARCH}/doFS.sh)
DOFS_SH=	${.CURDIR}/${TARGET_ARCH}/doFS.sh
.elif ${TARGET} != ${MACHINE}
DOFS_SH=	${.CURDIR}/scripts/doFS.sh ${DISKLABEL} ${TARGET}
.else
DOFS_SH=	${.CURDIR}/scripts/doFS.sh ${DISKLABEL} ""
.endif

.if ${TARGET_ARCH} == "sparc64"
CRUNCH_TARGETS= boot
.elif ${TARGET_ARCH} == "ia64"
CRUNCH_TARGETS= boot
.elif ${TARGET} == "pc98"
CRUNCH_TARGETS=	boot fixit fixit-small
.elif ${TARGET_ARCH} == "amd64"
CRUNCH_TARGETS= boot
.endif

CRUNCH_TARGETS?=boot fixit

EXTRAS= ftp.1
.if !defined(NOCDROM)
EXTRAS+= cdrom.1
.if defined(MAKE_ISOS)
EXTRAS+= iso.1
.endif
.if ${TARGET} == "i386" && defined(EMUL_BOOT)
BOOTABLE="-eb"
.elif ${TARGET} == "pc98"
BOOTABLE=
.else
BOOTABLE="-b"
.endif
.endif

.if !defined(NODOC)
DOCREL= doc.1 doc.2
.endif

BINMAKE!=	cd ${.CURDIR}/..; ${MAKE} -V BINMAKE
WMAKEENV!=	cd ${.CURDIR}/..; \
		${BINMAKE} ${CROSSENV} -f Makefile.inc1 -V WMAKEENV
WMAKE=		${WMAKEENV} ${BINMAKE}

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
.if make(release)
.if exists(${CHROOTDIR})
# The first command will fail on a handful of files that have their schg
# flags set.  But it greatly speeds up the next two commands.
	# NB: clear any vestigal devfs mount, just in case
	-umount ${CHROOTDIR}/dev > /dev/null 2>&1
	-rm -rf ${CHROOTDIR} 2>/dev/null
	-chflags -R noschg ${CHROOTDIR}/.
	-rm -rf ${CHROOTDIR}
.endif
	mkdir -p ${CHROOTDIR}
	@echo ">>> make release for ${TARGET} started on `LC_ALL=C TZ=GMT date`"
	cd ${WORLDDIR} && ${NATIVEMAKE} -DNOGAMES -DNOHTML -DNOINFO -DNOMAN \
	    -DNOPROFILE installworld DESTDIR=${CHROOTDIR}
	cd ${WORLDDIR}/etc && ${NATIVEMAKE} distribution DESTDIR=${CHROOTDIR}
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
		${CVSPREFIX} cvs -R ${CVSARGS} -d ${CVSROOT} co ${CVSCMDARGS} ${CVS_SRCARGS} ${RELEASESRCMODULE}
.endif
.if defined(LOCAL_PATCHES) && exists(${LOCAL_PATCHES})
	cd ${CHROOTDIR}/usr/src && patch ${PATCH_FLAGS} < ${LOCAL_PATCHES}
.endif
.if defined(LOCAL_SCRIPT) && exists(${LOCAL_SCRIPT})
	cd ${CHROOTDIR} && env CHROOTDIR=${CHROOTDIR} BUILDNAME=${BUILDNAME} RELEASETAG=${RELEASETAG} ${LOCAL_SCRIPT}
.endif
	rm -rf ${CHROOTDIR}/usr/ports
.if !defined(NOPORTSATALL)
	cd ${CHROOTDIR}/usr && ${CVSPREFIX} cvs -R ${CVSARGS} -d ${CVSROOT} co ${CVSCMDARGS} ${CVS_PORTSARGS} ${RELEASEPORTSMODULE}
.endif
.if !defined(NODOC)
	rm -rf ${CHROOTDIR}/usr/doc
.if defined(EXTDOCDIR)
	cd ${CHROOTDIR}/usr && cp -R -H ${EXTDOCDIR} doc
.else
	cd ${CHROOTDIR}/usr && ${CVSPREFIX} cvs -R ${CVSARGS} -d ${CVSROOT} co ${CVSCMDARGS} ${CVS_DOCARGS} ${RELEASEDOCMODULE}
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
.if !defined(RELEASENOUPDATE)
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr/src && ${CVSPREFIX} cvs -R ${CVSARGS} -q update ${CVSCMDARGS} -P -d -A
.else
	cd ${CHROOTDIR}/usr/src && ${CVSPREFIX} cvs -R ${CVSARGS} -q update ${CVSCMDARGS} -P -d -r ${RELEASETAG}
.endif
	rm -f ${CHROOTDIR}/tmp/.world_done 
.if !defined(NOPORTS)
	cd ${CHROOTDIR}/usr/ports && ${CVSPREFIX} cvs -R ${CVSARGS} -q update ${CVSCMDARGS} -P -d
.endif
.if defined(DOMINIMALDOCPORTS) && ${DOMINIMALDOCPORTS} == "YES"
	for i in ${MINIMALDOCPORTS}; do \
		( cd ${CHROOTDIR}/usr/$$i && ${CVSPREFIX} cvs -R ${CVSARGS} -q update ${CVSCMDARGS} -P -d ) ; \
	done
.endif
.if !defined(NODOC)
	cd ${CHROOTDIR}/usr/doc && ${CVSPREFIX} cvs -R ${CVSARGS} -q update ${CVSCMDARGS} -P -d
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
	echo "#!/bin/sh"			> ${CHROOTDIR}/mk
	echo "set -ex"				>> ${CHROOTDIR}/mk
	echo "trap 'umount /dev || true' 0"	>> ${CHROOTDIR}/mk
	echo "_RELTARGET=\$${1:-doRELEASE}"	>> ${CHROOTDIR}/mk
.for var in \
	AUTO_KEYBOARD_DETECT \
	BOOT_CONFIG \
	BUILDNAME \
	CD_EXTRA_BITS \
	DISTRIBUTIONS \
	DOC_LANG \
	DOMINIMALDOCPORTS \
	EXTRA_SRC \
	FIXCRYPTO \
	FTP_PASSIVE_MODE \
	FTP_PROXY \
	HTTP_PROXY \
	KERNELS \
	KERNEL_FLAGS \
	MAKE_ISOS \
	NOCDROM \
	NODOC \
	NOPORTS \
	NOSHARED \
	NOSRC \
	NO_CPU_CFLAGS \
	NO_CPU_COPTFLAGS \
	NO_FLOPPIES \
	NO_SENDMAIL \
	RELEASETAG \
	RELNOTES_LANG \
	TARGET \
	TARGET_ARCH \
	WORLD_FLAGS
.if defined(${var})
	echo "export ${var}=\"${${var}}\""	>> ${CHROOTDIR}/mk
.endif
.endfor
	# Don't remove this, or the build will fall over!
	echo "export RELEASEDIR=${_R}"		>> ${CHROOTDIR}/mk
	echo "export PATH=/bin:/usr/bin:/sbin:/usr/sbin:${LOCALDIR}"	>> ${CHROOTDIR}/mk
	echo "export MANBUILDCAT=YES"		>> ${CHROOTDIR}/mk
	# NB: these may fail if the host is running w/o devfs
	echo "umount /dev >/dev/null 2>&1 || true" >> ${CHROOTDIR}/mk
	echo "mount -t devfs devfs /dev >/dev/null 2>&1 || true" \
						>> ${CHROOTDIR}/mk
	echo "if [ ! -f /tmp/.world_done ]; then" >> ${CHROOTDIR}/mk
	echo "	cd /usr/src"			>> ${CHROOTDIR}/mk
	echo "	${CROSSMAKE} ${WORLD_FLAGS} -DNOCLEAN buildworld && \\" >> ${CHROOTDIR}/mk
	echo "	touch /tmp/.world_done"		>> ${CHROOTDIR}/mk
	echo "fi"				>> ${CHROOTDIR}/mk
	echo "if [ ! -f /tmp/.skip_ports ]; then" >> ${CHROOTDIR}/mk
	echo "	echo \">>> make readmes started on \`LC_ALL=C TZ=GMT date\`\"" >> ${CHROOTDIR}/mk
	echo "	cd /usr/ports"			>> ${CHROOTDIR}/mk
	echo "	make ${PORTREADMES_FLAGS} readmes" >> ${CHROOTDIR}/mk
	echo "	touch /tmp/.skip_ports"		>> ${CHROOTDIR}/mk
	echo "	echo \">>> make readmes finished on \`LC_ALL=C TZ=GMT date\`\"" >> ${CHROOTDIR}/mk
	echo "fi"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release"		>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make \$${_RELTARGET}"		>> ${CHROOTDIR}/mk
	echo "echo \">>> make ${.TARGET} for ${TARGET} finished on \`LC_ALL=C TZ=GMT date\`\"" >> ${CHROOTDIR}/mk
	chmod 755 ${CHROOTDIR}/mk
.if defined(NOPORTS) || defined(NOPORTREADMES)
	touch ${CHROOTDIR}/tmp/.skip_ports
.endif
	# Ensure md.ko is loaded if md(4) is not statically compiled into the kernel
	-mdconfig 2>/dev/null
	env -i /usr/sbin/chroot ${CHROOTDIR} /mk

clean:
	rm -rf ${CRUNCH_TARGETS:S/$/_crunch/} release.[0-9] release.10 \
	    ${EXTRAS}

fetch-distfiles:
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
.if !defined(NO_FLOPPIES)
	mkdir ${RD}/floppies
.endif
	mkdir ${RD}/trees
	mkdir ${RD}/kernels
	for i in ${DISTRIBUTIONS} ; do \
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
	touch ${.TARGET}

# Install the system into the various distributions.
release.2:
	cd ${.CURDIR}/../etc && make distrib-dirs DESTDIR=${RD}/trees/base
	cd ${.CURDIR}/.. && ${CROSSMAKE} distributeworld DISTDIR=${RD}/trees
	touch ${.TARGET}

# Build and install crypto distributions.
release.3:
	# Handle some grief caused by the munition braindeadness.
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${WORLD_FLAGS} -DNO_MAKEDB_RUN -DNOCRYPT \
	    SUBDIR_OVERRIDE="${FIXCRYPTO}" \
	    buildworld distributeworld DISTDIR=${RD}/trees
	-chflags -R noschg ${RD}/trees
	touch ${.TARGET}

# Make and install the generic kernel(s).
release.4:
.for kernel in ${KERNELS}
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${KERNEL_FLAGS} -DNO_MODULES \
	    KERNCONF=${kernel} INSTKERNNAME=${kernel} kernel \
	    DESTDIR=${RD}/trees/base
.endfor
	# Install a standard boot kernel+modules.
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${KERNEL_FLAGS} \
	    kernel \
	    DESTDIR=${RD}/trees/base; \
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${KERNEL_FLAGS} -DNO_MODULES \
	    reinstallkernel -DINSTALL_DEBUG \
	    DESTDIR=${RD}/trees/base
	touch ${.TARGET}

# Make and install the three crunched binaries which live on the floppies.
# You are not supposed to like this :-)
release.5:
	rm -rf ${RD}/crunch
	mkdir -p ${RD}/crunch
.for j in ${CRUNCH_TARGETS}
	rm -rf ${j}_crunch
	mkdir ${j}_crunch
.if exists(${.CURDIR}/${TARGET}/${j}_crunch.conf)
	cd ${j}_crunch; ${WMAKEENV} crunchgen -o ${.CURDIR}/${TARGET}/${j}_crunch.conf
.else
	cd ${j}_crunch; ${WMAKEENV} crunchgen -o ${.CURDIR}/${j}_crunch.conf
.endif
	cd ${j}_crunch; ${WMAKE} -f ${j}_crunch.mk subclean
	cd ${.CURDIR}/..; ${BINMAKE} -f Makefile.inc1 _build-tools
	cd ${j}_crunch; ${WMAKE} -f ${j}_crunch.mk \
	    CFLAGS="-Os -pipe" -DNO_CPU_CFLAGS all
	${WMAKEENV} strip -R .comment ${j}_crunch/${j}_crunch
	mv ${j}_crunch/${j}_crunch ${RD}/crunch/${j}
.endfor
	touch ${.TARGET}

#
# --==## Fix up the distributions. ##==--
#
release.6:
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
		find ${OTHER_DISTS} ${COMPAT_DISTS} ${CRYPTO_DISTS} -depth -type d -print | xargs rmdir
	touch ${.TARGET}

#
# --==## Package up the tarballs from assembled trees ##==--
#
release.7:
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
	touch ${.TARGET}


#
# --==## Make source dists ##==--
#
release.8:
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
	if [ -d ${RD}/dists/crypto ] ; then ( cd ${RD}/dists/src && \
		if [ -f ssecure.aa ] ; then mv ssecure.* ../crypto ; fi && \
		if [ -f scrypto.aa ] ; then mv scrypto.* ../crypto ; fi && \
		if [ -f skrb5.aa ] ; then mv skrb5.* ../crypto ; fi ; \
		cd ${RD}/dists/crypto; rm -f CHECKSUM.MD5; \
		md5 * > CHECKSUM.MD5 ) ; fi
	(cd ${RD}/dists/src; rm -f CHECKSUM.MD5; md5 * > CHECKSUM.MD5)
	@echo "src distribution is finished."
.endif
	touch ${.TARGET}

# Complete the bootfd
#
# Now, just to get this picture down once and for all:
#
# +------------------------------------------------------------------------+
# |boot.flp                                                                |
# +-----+-----+------------------------------------------------------------+
# |boot1|boot2|floppy filesystem "bootfd"                                  |
# +-----+-----+-+----------------------+---------------------------------+-+
#               |kernel.gz             |mfsroot.gz                       |
#               +----------------------+---------------------------------+
#

release.9:
	cp ${RD}/trees/base/etc/disktab /etc
	rm -rf ${RD}/mfsfd
	mkdir ${RD}/mfsfd
	cd ${RD}/mfsfd && \
		mkdir -p etc/defaults dev mnt stand/etc/defaults stand/help
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=boot \
		DIR=${RD}/mfsfd/stand ZIP=false
	( cd ${RD}/mfsfd && \
	  for dir in bin sbin ; do \
		ln -sf /stand $$dir; \
	  done )
	cp ${RD}/trees/base/sbin/dhclient-script ${RD}/mfsfd/stand
.if ${TARGET} == "pc98"
	cp ${.CURDIR}/../etc/defaults/pccard.conf ${RD}/mfsfd/etc/defaults/pccard.conf
.endif
	cp ${.CURDIR}/../etc/usbd.conf ${RD}/mfsfd/etc/usbd.conf
	( for F in defaults/rc.conf netconfig protocols ; do \
		sed -e '/^#.*$$/d' -e 's/[:space:]*#.*$$//g' \
		${RD}/trees/base/etc/$$F > ${RD}/mfsfd/stand/etc/$$F ; \
	  done )
	grep -E '^(ftp|nameserver|domain|sunrpc|cmd|nfsd)[^-\w]' \
	    ${RD}/trees/base/etc/services | \
	    sed -e '/^#.*$$/d' -e 's/[:space:]*#.*$$//g' \
	    > ${RD}/mfsfd/stand/etc/services
	ln ${RD}/mfsfd/stand/etc/services ${RD}/mfsfd/etc/services
	ln ${RD}/mfsfd/stand/etc/netconfig ${RD}/mfsfd/etc/netconfig
	cp ${RD}/trees/base/COPYRIGHT ${RD}/mfsfd/stand/help/COPYRIGHT.hlp
.if !defined(NODOC)
	@for i in ${DIST_DOCS_ARCH_INDEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.txt ${RD}/mfsfd/stand/help/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.txt ${RD}/mfsfd/stand/help/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	done
	@mv ${RD}/mfsfd/stand/help/INSTALLATION.TXT ${RD}/mfsfd/stand/help/INSTALL.TXT
	@mv ${RD}/mfsfd/stand/help/EARLY-ADOPTER.TXT ${RD}/mfsfd/stand/help/EARLY.TXT
.endif
	-test -f ${.CURDIR}/install.cfg && cp ${.CURDIR}/install.cfg ${RD}/mfsfd
	@mkdir -p ${RD}/mfsfd/boot
.if ${TARGET_ARCH} != "ia64"
	@cp ${RD}/trees/base/boot/boot* ${RD}/mfsfd/boot
.endif
.if ${TARGET} == "i386" || ${TARGET_ARCH} == "amd64"
	@cp ${RD}/trees/base/boot/mbr ${RD}/mfsfd/boot
.endif
	@tar --exclude CVS -cf - -C ${.CURDIR}/../usr.sbin/sysinstall help | \
		tar xf - -C ${RD}/mfsfd/stand
.if ${TARGET_ARCH} == "alpha" && !defined(NO_FLOPPIES)
	rm -rf ${RD}/mfsfd/stand/help/*
.endif
	@mkdir -p ${RD}/mfsroot
.if defined(SMALLBOOTSIZE)
.if exists(${.CURDIR}/${TARGET}/drivers-small.conf)
	@mkdir -p ${RD}/mfsfd/modules
	@awk -f  ${.CURDIR}/scripts/driver-copy2.awk 2 \
	    ${.CURDIR}/${TARGET}/drivers-small.conf \
	    ${RD}/trees/base/boot/kernel ${RD}/mfsfd/modules
.endif
	sh -e ${DOFS_SH} mfsroot-small ${RD} ${MNT} \
		${MFSSIZE} ${RD}/mfsfd ${MFSINODE} ${MFSLABEL}
	@gzip -9vc mfsroot-small > ${RD}/mfsroot/mfsroot-small.gz
	@rm -rf ${RD}/mfsfd/modules
.endif
.if exists(${.CURDIR}/${TARGET}/drivers.conf)
	@mkdir -p ${RD}/mfsfd/modules
	@awk -f  ${.CURDIR}/scripts/driver-copy2.awk 2 \
	    ${.CURDIR}/${TARGET}/drivers.conf \
	    ${RD}/trees/base/boot/kernel ${RD}/mfsfd/modules
.endif
	sh -e ${DOFS_SH} mfsroot ${RD} ${MNT} \
		${MFSSIZE} ${RD}/mfsfd ${MFSINODE} ${MFSLABEL}
	@gzip -9vc mfsroot > ${RD}/mfsroot/mfsroot.gz
	@rm -f mfsroot mfsroot-small
	touch ${.TARGET}

release.10:
.if !defined(NO_FLOPPIES)
.if exists(${.CURDIR}/${TARGET}/drivers.conf)
	@echo "Making the drivers floppy."
	@rm -rf ${RD}/driversfd
	@mkdir ${RD}/driversfd
	@awk -f  ${.CURDIR}/scripts/driver-copy2.awk 3 \
	    ${.CURDIR}/${TARGET}/drivers.conf \
	    ${RD}/trees/base/boot/kernel ${RD}/driversfd
	-@rmdir ${RD}/driversfd
	if [ -d ${RD}/driversfd ]; then \
	    sh -e ${DOFS_SH} \
		${RD}/floppies/drivers.flp ${RD} ${MNT} ${DRIVERSIZE} \
		${RD}/driversfd ${DRIVERINODE} ${DRIVERLABEL}; \
	    cd ${RD}/driversfd && awk -f  ${.CURDIR}/scripts/driver-desc.awk \
		*.dsc >> ${RD}/floppies/DRIVERS.TXT; \
	fi
.endif
.if defined(BOOTSIZE)
	@echo "Making the regular boot floppy."
	@cd ${.CURDIR} && ${MAKE} doMFSKERN FSIMAGE=kern
.endif
.if defined(SMALLBOOTSIZE)
	@echo "Making the small boot floppy."
	@cd ${.CURDIR} && ${MAKE} doMFSKERN FSIMAGE=kern-small FDSIZE=SMALL
.endif
.if defined(BIGBOOTSIZE)
	@echo "Making the big boot floppy."
	@cd ${.CURDIR} && ${MAKE} doMFSKERN FSIMAGE=boot FDSIZE=BIG
.endif
.if defined(FIXITSIZE) || defined(SMALLFIXITSIZE)
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
.if defined(SMALLFIXITSIZE)
	@cd ${.CURDIR} && ${MAKE} installCRUNCH CRUNCH=fixit-small \
		DIR=${RD}/fixitfd/stand ZIP=false
	@sh -e ${DOFS_SH} ${RD}/floppies/fixit-small.flp \
		${RD} ${MNT} ${SMALLFIXITSIZE} ${RD}/fixitfd ${FIXITINODE} \
		${SMALLFIXITLABEL}
	@rm -rf ${RD}/fixitfd/stand
	@mkdir ${RD}/fixitfd/stand
.endif
.if defined(FIXITSIZE)
	@cd ${.CURDIR} && ${MAKE} installCRUNCH CRUNCH=fixit \
		DIR=${RD}/fixitfd/stand ZIP=false
	@sh -e ${DOFS_SH} ${RD}/floppies/fixit.flp ${RD} \
		${MNT} ${FIXITSIZE} ${RD}/fixitfd ${FIXITINODE} ${FIXITLABEL}
.endif
.endif
# Do our last minute floppies directory setup in a convenient place.
.if !defined(NODOC)
	@cp ${RND}/${RELNOTES_LANG}/installation/${TARGET}/article.txt \
		${RD}/floppies/README.TXT
	@(cd ${RD}/floppies; md5 README.TXT *.flp > CHECKSUM.MD5)
.else
	@(cd ${RD}/floppies; md5 *.flp > CHECKSUM.MD5)
.endif
.endif
	touch ${.TARGET}

#
# --==## Setup a suitable ftp-area ##==--
#
ftp.1:
	@echo "Setting up FTP distribution area"
	@mkdir -p ${FD}
	-@ln -s . ${FD}/${BUILDNAME}
.if !defined(NO_FLOPPIES)
	@cd ${RD} && find floppies -print | cpio -dumpl ${FD}
.endif
	@cd ${RD}/dists && find . -print | cpio -dumpl ${FD}
.if !defined(NODOC)
	@for i in ${DIST_DOCS_ARCH_INDEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.txt ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.html ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.txt ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.html ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@cp ${RND}/${RELNOTES_LANG}/readme/docbook.css ${FD}
	@mv ${FD}/INSTALLATION.TXT ${FD}/INSTALL.TXT
	@mv ${FD}/INSTALLATION.HTM ${FD}/INSTALL.HTM
	@mv ${FD}/EARLY-ADOPTER.TXT ${FD}/EARLY.TXT
	@mv ${FD}/EARLY-ADOPTER.HTM ${FD}/EARLY.HTM
.endif
	@echo "CD_VERSION = ${BUILDNAME}" > ${FD}/cdrom.inf
.if !defined(NOPORTS)
	@tar --exclude CVS -cBf - -C ${CD_DISC1} ports | tar -xBf - -C ${FD}
.endif
	touch ${.TARGET}

#
# --==## Setup a suitable cdrom-area ##==--
#
cdrom.1:
	@echo "Setting up CDROM distribution area"
	@mkdir -p ${CD_DISC1} ${CD_DISC2}
	-@ln -s . ${CD_DISC1}/${BUILDNAME}
.if !defined(NO_FLOPPIES)
	@cd ${RD} && find floppies -print | cpio -dumpl ${CD_DISC1}
.endif
	@cd ${RD}/dists && find . -print | cpio -dumpl ${CD_DISC1}
	@for i in ${DISTRIBUTIONS} ; \
	do \
		if [ -d ${RD}/trees/$${i} ] ; then \
			chflags -R noschg ${RD}/trees/$${i} || true ; \
			( cd ${RD}/trees/$${i} && \
			find . -depth -print | cpio -dumpl ${CD_DISC2} ) ; \
		fi \
	done
	@rm -f ${CD_DISC2}/.profile
	@cp ${.CURDIR}/fixit.profile ${CD_DISC2}/.profile
	@echo "CD_VERSION = ${BUILDNAME}" > ${CD_DISC1}/cdrom.inf
	@echo "CD_VERSION = ${BUILDNAME}" > ${CD_DISC2}/cdrom.inf
.if !defined(NODOC)
	@for i in ${DIST_DOCS_ARCH_INDEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.txt ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.html ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.txt ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET}/article.html ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@cp ${RND}/${RELNOTES_LANG}/readme/docbook.css ${CD_DISC1}
	@mv ${CD_DISC1}/INSTALLATION.TXT ${CD_DISC1}/INSTALL.TXT
	@mv ${CD_DISC1}/INSTALLATION.HTM ${CD_DISC1}/INSTALL.HTM
	@mv ${CD_DISC1}/EARLY-ADOPTER.TXT ${CD_DISC1}/EARLY.TXT
	@mv ${CD_DISC1}/EARLY-ADOPTER.HTM ${CD_DISC1}/EARLY.HTM
.endif
.if ${TARGET} != "pc98"
	@echo "Setting up /boot"
	@rm -f ${CD_DISC2}/boot/loader.conf
	@cp ${RD}/mfsroot/mfsroot.gz ${CD_DISC2}/boot/mfsroot.gz
	@echo 'mfsroot_load="YES"' > ${CD_DISC2}/boot/loader.conf
	@echo 'mfsroot_type="mfs_root"' >> ${CD_DISC2}/boot/loader.conf
	@echo 'mfsroot_name="/boot/mfsroot"' >> ${CD_DISC2}/boot/loader.conf
	@cp -Rp ${CD_DISC2}/boot ${CD_DISC1}
.endif
.if ${TARGET} == "i386" && defined(EMUL_BOOT) && !defined(NO_FLOPPIES)
	# In case bootable ISO with floppy emulation is desired.
	@mkdir -p ${CD_DISC2}/floppies
	@cp ${CD_DISC1}/floppies/boot.flp ${CD_DISC2}/floppies
.endif
.if !defined(NOPORTS)
	@mkdir -p ${CD_DISC1}/ports && \
	  tar --exclude CVS --exclude 'ports/distfiles/*' -czf \
	  ${CD_DISC1}/ports/ports.tgz -C /usr ports && \
	  cp ${.CURDIR}/scripts/ports-install.sh ${CD_DISC1}/ports/install.sh \
	  && (cd ${CD_DISC1}/ports; md5 * > CHECKSUM.MD5)
.endif
	touch ${.TARGET}

iso.1:
.if exists(${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh)
	@echo "Creating ISO images..."
	@sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh ${BOOTABLE} \
	    fbsd_miniinst \
	    ${CD}/${BUILDNAME}-${TARGET}-miniinst.iso ${CD_DISC1}
	@sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh ${BOOTABLE} \
	    fbsd_livefs \
	    ${CD}/${BUILDNAME}-${TARGET}-disc2.iso ${CD_DISC2}
.if defined(CD_EXTRA_BITS)
	@sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh \
	    ${BOOTABLE} fbsd_boot \
	    ${CD}/${BUILDNAME}-${TARGET}-disc1.iso ${CD_DISC1} \
	    ${CD_EXTRA_BITS} \
	    && false
.endif
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
	@cd /usr/doc && make all install 'FORMATS=html html-split txt' INSTALL_COMPRESSED='' DOCDIR=${RD}/trees/base/usr/share/doc
	touch ${.TARGET}

#
# --==## RELNOTESng:  Next-generation replacements for *.TXT files ##==--
#
doc.2:
	@echo "Making release documentation..."
	@cd ${.CURDIR}/doc && make all install clean 'FORMATS=html txt' INSTALL_COMPRESSED='' DOCDIR=${RND}
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
		tn=`echo ${TN} | tr '[A-Z]' '[a-z]' | cut -c1-8` && \
		echo rolling ${TD}/$$tn tarball &&\
		tar --exclude CVS --exclude obj --exclude BOOTMFS -cf - ${ARG} | \
		${ZIPNSPLIT} ${RD}/dists/${TD}/$$tn. && \
		sh ${.CURDIR}/scripts/info.sh ${RD}/dists/${TD}/$$tn > \
		   ${RD}/dists/${TD}/$$tn.inf && \
		if [ -f ${.CURDIR}/scripts/$${TD}-install.sh ]; then \
		  cp -p ${.CURDIR}/scripts/$${TD}-install.sh ${RD}/dists/${TD}/install.sh; \
		fi && \
		if [ "${SD}" != "/usr/src" ]; then \
			mtree -c -i -p ${SD}/${ARG} \
			  -k gname,md5digest,mode,nlink,uname,size,link,type \
			  > ${RD}/dists/${TD}/$$tn.mtree ; \
		else \
			true; \
		fi; \
		(cd ${RD}/dists/${TD}; \
		   rm -f CHECKSUM.MD5; \
		   md5 * > CHECKSUM.MD5) \
	)

doRELEASE:  release.1 release.2 release.3 ${DOCREL} release.4 release.5 \
		release.6 release.7 release.8 release.9 release.10
	@cd ${.CURDIR} && ${MAKE} ${EXTRAS}
	@echo "Release done"

floppies:
	@rm -f release.5 release.9 release.10
	@cd ${.CURDIR} && ${MAKE} release.5 release.9 release.10
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
# --==## Put a filesystem into a BOOTMFS kernel ##==--
#
doMFSKERN:
	@echo "Running ${.TARGET} for ${FSIMAGE}"
	@rm -f ${RD}/kernels/BOOTMFS ${RD}/kernels/BOOTMFS.${FSIMAGE}
	@cd ${.CURDIR}/../sys/${TARGET}/conf; \
	if [ -r ${.CURDIR}/${TARGET}/dokern.sh ]; then \
	    sh ${.CURDIR}/${TARGET}/dokern.sh ${FDSIZE}; \
	else \
	    sed -e 's/ident.*GENERIC/ident		BOOTMFS/g'; \
	fi < GENERIC > BOOTMFS; \
	[ -r GENERIC.hints ] && cp GENERIC.hints BOOTMFS.hints
.if defined(FDSIZE) && ${FDSIZE} == "SMALL" && \
    exists(${.CURDIR}/${TARGET}/drivers-small.conf)
	@awk -f ${.CURDIR}/scripts/driver-remove.awk \
		${.CURDIR}/${TARGET}/drivers-small.conf \
		${.CURDIR}/../sys/${TARGET}/conf/BOOTMFS
.elif exists(${.CURDIR}/${TARGET}/drivers.conf)
	@awk -f ${.CURDIR}/scripts/driver-remove.awk \
		${.CURDIR}/${TARGET}/drivers.conf \
		${.CURDIR}/../sys/${TARGET}/conf/BOOTMFS
.endif
	cd ${.CURDIR}/..; \
	KERNEL_KO=BOOTMFS KODIR= \
	    ${CROSSMAKE} ${KERNEL_FLAGS} -DNO_MODULES -DNO_KERNELCLEAN \
	    KERNCONF=BOOTMFS COPTFLAGS="-Os -pipe" -DNO_CPU_COPTFLAGS \
	    buildkernel reinstallkernel \
	    DESTDIR=${RD}/kernels
	[ -r ${.CURDIR}/../sys/${TARGET}/conf/BOOTMFS.hints ] && \
	cp ${.CURDIR}/../sys/${TARGET}/conf/BOOTMFS.hints ${RD}/kernels
	@rm -rf ${RD}/image.${FSIMAGE}
	@mkdir ${RD}/image.${FSIMAGE}
	@cd ${RD}/kernels && \
	  (chflags noschg BOOTMFS || true) && \
	  ${WMAKEENV} strip -R .comment BOOTMFS && \
	  cp BOOTMFS BOOTMFS.${FSIMAGE} && \
	  [ -r BOOTMFS.hints ] && mv BOOTMFS.hints BOOTMFS.${FSIMAGE}.hints
	mv ${RD}/kernels/BOOTMFS ${RD}/image.${FSIMAGE}/kernel
	@echo "Setting up /boot directory for ${FSIMAGE} floppy"
	@mkdir -p ${RD}/image.${FSIMAGE}/boot
.if ${TARGET} == "i386"
	@${WMAKEENV} kgzip -v -l ${RD}/trees/base/usr/lib/kgzldr.o \
	    -o ${RD}/image.${FSIMAGE}/boot/loader ${RD}/trees/base/boot/loader
.elif ${TARGET_ARCH} == "ia64"
	@cp ${RD}/trees/base/boot/loader.efi ${RD}/image.${FSIMAGE}/boot
.else
	@cp ${RD}/trees/base/boot/loader ${RD}/image.${FSIMAGE}/boot
.endif
	@[ -r ${RD}/kernels/BOOTMFS.${FSIMAGE}.hints ] && \
	  sed -e '/^hint/s/^/set /' -e '/^#/d' \
	    ${RD}/kernels/BOOTMFS.${FSIMAGE}.hints > \
	    ${RD}/image.${FSIMAGE}/boot/device.hints && \
	  echo "include /boot/device.hints" > ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "load /kernel" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.if !defined(FDSIZE) || ${FDSIZE} != "BIG"
	@echo "echo \\007\\007" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "echo Please insert MFS root floppy and press enter:" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "read" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.endif
.if defined(FDSIZE) && ${FDSIZE} == "SMALL"
	@echo "load -t mfs_root /mfsroot-small" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.else
	@echo "load -t mfs_root /mfsroot" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.endif
.if ${TARGET_ARCH} != "ia64"
	@echo "set hint.acpi.0.disabled=1" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.endif
	@if [ -r ${RD}/floppies/drivers.flp ]; then \
	    echo "set driver_floppy=YES" >> ${RD}/image.${FSIMAGE}/boot/loader.rc; \
	fi
	@echo "set module_path=\"/modules;/dist\"" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "echo \\007\\007" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "autoboot 10" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.if ${TARGET_ARCH} == "i386" && ${AUTO_KEYBOARD_DETECT}
	@echo "-P" >> ${RD}/image.${FSIMAGE}/boot.config
.endif
	@gzip -9v ${RD}/image.${FSIMAGE}/kernel
	@rm -f ${RD}/floppies/${FSIMAGE}.flp
.if defined(FDSIZE) && ${FDSIZE} == "BIG"
	@cp ${RD}/mfsroot/mfsroot.gz ${RD}/image.${FSIMAGE}
	sh -e ${DOFS_SH} ${RD}/floppies/${FSIMAGE}.flp \
		${RD} ${MNT} ${BIGBOOTSIZE} ${RD}/image.${FSIMAGE} \
		${BOOTINODE} ${BIGBOOTLABEL}
.elif defined(FDSIZE) && ${FDSIZE} == "SMALL"
	sh -e ${DOFS_SH} ${RD}/floppies/mfsroot-small.flp \
		${RD} ${MNT} ${SMALLBOOTSIZE} ${RD}/mfsroot/mfsroot-small.gz \
		${BOOTINODE} ${SMALLBOOTLABEL}
	sh -e ${DOFS_SH} ${RD}/floppies/${FSIMAGE}.flp \
		${RD} ${MNT} ${SMALLBOOTSIZE} ${RD}/image.${FSIMAGE} \
		${BOOTINODE} ${SMALLBOOTLABEL}
.else
	sh -e ${DOFS_SH} ${RD}/floppies/mfsroot.flp \
		${RD} ${MNT} ${BOOTSIZE} ${RD}/mfsroot/mfsroot.gz \
		${BOOTINODE} ${BOOTLABEL}
	sh -e ${DOFS_SH} ${RD}/floppies/${FSIMAGE}.flp \
		${RD} ${MNT} ${BOOTSIZE} ${RD}/image.${FSIMAGE} \
		${BOOTINODE} ${BOOTLABEL}
.endif
	@echo "Created ${RD}/floppies/${FSIMAGE}.flp"

.include <bsd.obj.mk>
