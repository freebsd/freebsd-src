# $FreeBSD$
#
# make release CHROOTDIR=/some/dir BUILDNAME=somename CVSROOT=/cvs/dir \
#	[ RELEASETAG=tag ]
#
# Where "/some/dir" is the pathname of a directory on a some filesystem with
# at least 1000MB of free space, "somename" is what you want the release to
# call itself, "/cvs/dir" is where our source repo resides and, optionally,
# which CVS "tag" name should be used when checking out the sources to build
# the release (default is HEAD).
#
# Please note: the vn driver must also be compiled into your kernel,
# otherwise the target 'release.8' and possibly others will fail.
#
# Note:  "/some/dir" cannot reside on a filesystem mounted with
# the "nodev" option, otherwise the chrooted "buildworld" will likely
# fail.
#
# Note: If you add options to this file, please keep release(7) updated!
#
# Set these, release builder!
#
# Fixed version:
#BUILDNAME=4.0-RELEASE
#
# Automatic SNAP versioning:
DATE != date +%Y%m%d
BASE = 4.6
BUILDNAME?=${BASE}-${DATE}-STABLE
#
#CHROOTDIR=/junk/release
# If this is a -stable snapshot, then set
#RELEASETAG=RELENG_4
# If you want to add other options to CVS commands, then set
#CVSCMDARGS="-D '01/01/2002 00:00:00 UTC'"
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
CROSSMAKE=	${MAKE} TARGET_ARCH=${TARGET_ARCH} TARGET=${TARGET}
NATIVEMAKE=	${MAKE} TARGET_ARCH=${MACHINE_ARCH} TARGET=${MACHINE}

# If you are using a local CVS repository with components stored in 
# non-standard modules, override these on the make commandline or
# in the environment.
RELEASESRCMODULE?=	src
RELEASEDOCMODULE?=	doc
RELEASEPORTSMODULE?=	ports

# Unless set elsewhere, indicate the object format we'll be using.
OBJFORMAT?=		elf

# Comment this out to enable port readmes to be built.
NOPORTREADMES?=	YES

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

# Uncomment and modify this definition if you want the release notes 
# and other release documentation in a language other than English.
#RELNOTES_LANG=	en_US.ISO8859-1

# As an alternative to installing the entire ports collection (which
# can take a huge amount of time, in particular on slower disks),
# setting ${MINIMALDOCPORTS} allows to install and build just those
# ports that are really required for getting the docs up & running.
.if defined(NOPORTS) && !defined(NODOC)
DOMINIMALDOCPORTS=	YES
.include "Makefile.inc.docports"
.endif

# Helper variable
.if defined(NOPORTS)
.if !defined(DOMINIMALDOCPORTS) || ${DOMINIMALDOCPORTS} != "YES"
NOPORTSATALL=	YES
.endif
.endif

# Set ALLLANG=no if you want the documentation (e.g. Handbook, FAQ) to be
# in English only.  The language for the release notes is controlled
# by the RELNOTES_LANG variable above.
ALLLANG?=	yes
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
DIST_DOCS_ARCH_INDEP=	readme errata
DIST_DOCS_ARCH_DEP=	installation relnotes hardware
.endif

# Things which without too much trouble can be considered variables
# BIN_DISTS are special in that they get full /etc installation sets.
#
.if ${TARGET_ARCH} == "i386"
COMPAT_DISTS?=	compat1x compat20 compat21 compat22 compat3x compat4x
.else
COMPAT_DISTS?=	compat4x
.endif
OTHER_DISTS?=	catpages manpages games proflibs dict info doc
CRYPTO_DISTS?=	crypto krb4 krb5
BIN_DISTS?=	bin
DISTRIBUTIONS?=	${BIN_DISTS} ${OTHER_DISTS} ${COMPAT_DISTS} ${CRYPTO_DISTS}

BOOT1=	etc/protocols etc/defaults/rc.conf

# mountpoint for filesystems.
MNT=			/mnt

# Various floppy image parameters.
#

.if ${TARGET_ARCH} == "i386"
.if ${TARGET} == "pc98"
SMALLBOOTSIZE=		1200
BOOTSIZE=		1440
FIXITSIZE=		1440
MFSSIZE=		4320
BOOTINODE=		80000
FIXITINODE=		4000
MFSINODE=		8000
SMALLBOOTLABEL=		fd1200
BOOTLABEL=		fd1440
FIXITLABEL=		fd1440
MFSLABEL=		minimum3
.else
BOOTSIZE=		1440
FIXITSIZE=		1440
MFSSIZE=		4320
BIGBOOTSIZE=		2880
BOOTINODE=		80000
FIXITINODE=		4000
MFSINODE=		8000
BOOTLABEL=		fd1440
FIXITLABEL=		fd1440
MFSLABEL=		minimum3
BIGBOOTLABEL=		minimum2
.endif
.elif ${TARGET_ARCH} == "alpha"
BOOTSIZE=		1440
FIXITSIZE=		2880
MFSSIZE=		4320
BIGBOOTSIZE=		2880
BOOTINODE=		80000
FIXITINODE=		4000
MFSINODE=		8000
BOOTLABEL=		fd1440
FIXITLABEL=		minimum2
MFSLABEL=		auto
BIGBOOTLABEL=		minimum2
.endif

ZIPNSPLIT=		gzip --no-name -9 -c | split -b 240640 -

# Things that need to be recompiled with Kerberos support.
K4PROGS!=	cd ${.CURDIR}/../kerberosIV; ${MAKE} -V KPROGS
K5PROGS!=	cd ${.CURDIR}/../kerberos5; ${MAKE} -V KPROGS

# Things that need to be compiled without crypto support in releases
.if !defined(FIXCRYPTO)
FIXCRYPTO=	bin/ed usr.sbin/ppp usr.sbin/pppd usr.sbin/tcpdump/tcpdump 
.if !defined(NO_SENDMAIL)
FIXCRYPTO+=	usr.sbin/sendmail
.endif
.endif


VNDEVICE?=		vn0

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

.if !defined(CRUNCH_TARGETS)
CRUNCH_TARGETS=	boot fixit
.endif

EXTRAS= cdrom.1 ftp.1
.if defined(MAKE_ISOS)
EXTRAS+= iso.1
.if ${TARGET} != "pc98"
BOOTABLE="-b"
.endif
.endif

.if !defined(NODOC)
DOCREL= doc.1 doc.2
.endif

.if !defined(NOPORTREADMES)
MAKEREADMES=	make readmes PORTSDIR=${CHROOTDIR}/usr/ports
.else
MAKEREADMES=	true
.endif

TMAKE!=		echo MAKEFLAGS=\"-m ${.CURDIR}/../share/mk\"; \
		cd ${.CURDIR}/..; ${MAKE} -f Makefile.inc1 -V TMAKE
WMAKEENV!=	echo MAKEFLAGS=\"-m ${.CURDIR}/../share/mk\"; \
		cd ${.CURDIR}/..; ${CROSSMAKE} -f Makefile.inc1 -V WMAKEENV
WMAKE=		${WMAKEENV} ${MAKE}

rerelease release:
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
	-rm -rf ${CHROOTDIR} 2>/dev/null
	-chflags -R noschg ${CHROOTDIR}/.
	-rm -rf ${CHROOTDIR}
.endif
	mkdir -p ${CHROOTDIR}
	@echo ">>> make release for ${TARGET} started on `LC_ALL=C TZ=GMT date`"
	cd ${.CURDIR}/.. && ${NATIVEMAKE} -DNOGAMES -DNOHTML -DNOINFO -DNOMAN \
	    -DNOPROFILE installworld DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/../etc && ${NATIVEMAKE} distribution DESTDIR=${CHROOTDIR}
	if [ -f /etc/resolv.conf ]; then \
		cp -p /etc/resolv.conf ${CHROOTDIR}/etc; \
	fi
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf src && \
		cvs -R -d ${CVSROOT} co ${CVSCMDARGS} -P ${RELEASESRCMODULE}
.else
	cd ${CHROOTDIR}/usr && rm -rf src && \
		cvs -R -d ${CVSROOT} co ${CVSCMDARGS} -P -r ${RELEASETAG} ${RELEASESRCMODULE}
.endif
.if defined(LOCAL_PATCHES) && exists(${LOCAL_PATCHES})
	cd ${CHROOTDIR}/usr/src && patch ${PATCH_FLAGS} < ${LOCAL_PATCHES}
.endif
.if defined(LOCAL_SCRIPT) && exists(${LOCAL_SCRIPT})
	cd ${CHROOTDIR} && env CHROOTDIR=${CHROOTDIR} BUILDNAME=${BUILDNAME} RELEASETAG=${RELEASETAG} ${LOCAL_SCRIPT}
.endif
.if !defined(NOPORTS)
.if defined(PORTSRELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co ${CVSCMDARGS} -P -r ${PORTSRELEASETAG} ${RELEASEPORTSMODULE} && cd ports && ${MAKEREADMES}
.else
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co ${CVSCMDARGS} -P ${RELEASEPORTSMODULE} && cd ports && ${MAKEREADMES}
.endif
.elif defined(DOMINIMALDOCPORTS) && ${DOMINIMALDOCPORTS} == "YES"
.if defined(PORTSRELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co ${CVSCMDARGS} -P -r ${PORTSRELEASETAG} ${MINIMALDOCPORTS}
.else
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co ${CVSCMDARGS} -P ${MINIMALDOCPORTS}
.endif
.endif
.if !defined(NODOC)
.if defined(DOCRELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf doc && cvs -R -d ${CVSROOT} co ${CVSCMDARGS} -P -r ${DOCRELEASETAG} ${RELEASEDOCMODULE}
.else
	cd ${CHROOTDIR}/usr && rm -rf doc && cvs -R -d ${CVSROOT} co ${CVSCMDARGS} -P ${RELEASEDOCMODULE}
.endif
	if [ -d ${RELEASEDISTFILES}/ ]; then \
		cp -rp ${RELEASEDISTFILES} ${CHROOTDIR}/usr/ports/distfiles; \
	else \
		mkdir -p ${CHROOTDIR}/usr/ports/distfiles; \
	fi
.endif
.endif
.if make(rerelease)
.if !defined(RELEASENOUPDATE)
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr/src && cvs -R -q update ${CVSCMDARGS} -P -d -A
.else
	cd ${CHROOTDIR}/usr/src && cvs -R -q update ${CVSCMDARGS} -P -d -r ${RELEASETAG}
.endif
.if !defined(NOPORTS)
	cd ${CHROOTDIR}/usr/ports && cvs -R -q update ${CVSCMDARGS} -P -d
.endif
.if defined(DOMINIMALDOCPORTS) && ${DOMINIMALDOCPORTS} == "YES"
	for i in ${MINIMALDOCPORTS}; do \
		( cd ${CHROOTDIR}/usr/$$i && cvs -R -q update ${CVSCMDARGS} -P -d ) ; \
	done
.endif
.if !defined(NODOC)
	cd ${CHROOTDIR}/usr/doc && cvs -R -q update ${CVSCMDARGS} -P -d
.endif
.endif
.endif
	# Add version information to those things that need it.
	( cd ${CHROOTDIR}/usr/src/sys/conf && \
	  mv newvers.sh foo && \
	  sed "s/^RELEASE=.*/RELEASE=${BUILDNAME}/" foo > newvers.sh && rm foo )
	echo OBJFORMAT=${OBJFORMAT} > ${CHROOTDIR}/etc/objformat
	-test -f install.cfg && cp install.cfg ${CHROOTDIR}/usr/src/release
	echo "#!/bin/sh"			> ${CHROOTDIR}/mk
	echo "set -ex"				>> ${CHROOTDIR}/mk
	echo "_RELTARGET=\$${1:-doRELEASE}"	>> ${CHROOTDIR}/mk
	echo "export WITHOUT_X11=YES"		>> ${CHROOTDIR}/mk
	echo "export DISTRIBUTIONS=\"${DISTRIBUTIONS}\"" >> ${CHROOTDIR}/mk
	echo "export BUILDNAME=${BUILDNAME}"	>> ${CHROOTDIR}/mk
	echo "export VNDEVICE=${VNDEVICE}"	>> ${CHROOTDIR}/mk
	echo "export OBJFORMAT=${OBJFORMAT}"	>> ${CHROOTDIR}/mk
	echo "export MODULES_WITH_WORLD=YES"	>> ${CHROOTDIR}/mk
.if defined(RELEASETAG)
	echo "export RELEASETAG=\"${RELEASETAG}\""	>> ${CHROOTDIR}/mk
.endif
.if defined(NOPORTS)
	echo "export NOPORTS=${NOPORTS}"	>> ${CHROOTDIR}/mk
.endif
.if defined(MAKE_ISOS)
	echo "export MAKE_ISOS=${MAKE_ISOS}"	>> ${CHROOTDIR}/mk
.endif
.if defined(DOMINIMALDOCPORTS)
	echo "export DOMINIMALDOCPORTS=${DOMINIMALDOCPORTS}" >> ${CHROOTDIR}/mk
.endif
.if defined(NODOC)
	echo "export NODOC=${NODOC}"		>> ${CHROOTDIR}/mk
.endif
.if defined(ALLLANG) && ${ALLLANG} != "NO" && ${ALLLANG} != "no"
	echo "export ALLLANG=${ALLLANG}"	>> ${CHROOTDIR}/mk
.else
	echo "export DOC_LANG=en_US.ISO8859-1"	>> ${CHROOTDIR}/mk
.endif
.if defined(NOSRC)
	echo "export NOSRC=${NOSRC}"		>> ${CHROOTDIR}/mk
.endif
.if defined(RELNOTES_LANG)
	echo "export RELNOTES_LANG=${RELNOTES_LANG}"	>> ${CHROOTDIR}/mk
.else
	echo "export RELNOTES_LANG=en_US.ISO8859-1"	>> ${CHROOTDIR}/mk
.endif
.if defined(NOSHARED)
	echo "export NOSHARED=${NOSHARED}"	>> ${CHROOTDIR}/mk
.endif
.if defined(BOOT_CONFIG)
	echo "export BOOT_CONFIG=\"${BOOT_CONFIG}\"">> ${CHROOTDIR}/mk
.endif
.if defined(WORLD_FLAGS)
	echo "export WORLD_FLAGS=\"${WORLD_FLAGS}\""	>> ${CHROOTDIR}/mk
.endif
.if defined(KERNEL_FLAGS)
	echo "export KERNEL_FLAGS=\"${KERNEL_FLAGS}\"" >> ${CHROOTDIR}/mk
.endif
.if defined(TARGET)
	echo "export TARGET=\"${TARGET}\"" >> ${CHROOTDIR}/mk
.endif
.if defined(TARGET_ARCH)
	echo "export TARGET_ARCH=\"${TARGET_ARCH}\"" >> ${CHROOTDIR}/mk
.endif
	# Don't remove this, or the build will fall over!
	echo "export RELEASEDIR=${_R}"		>> ${CHROOTDIR}/mk
	echo "export PATH=/bin:/usr/bin:/sbin:/usr/sbin:${LOCALDIR}"	>> ${CHROOTDIR}/mk
	echo "export TMPDIR=/tmp"		>> ${CHROOTDIR}/mk
	echo "export MAKEOBJDIRPREFIX=/usr/obj"	>> ${CHROOTDIR}/mk
	echo "export MANBUILDCAT=YES"		>> ${CHROOTDIR}/mk
	echo "if [ ! -f /tmp/.world_done ]; then" >> ${CHROOTDIR}/mk
	echo "	cd /usr/src"			>> ${CHROOTDIR}/mk
	echo "	${CROSSMAKE} ${WORLD_FLAGS} -DNOCLEAN buildworld && \\" >> ${CHROOTDIR}/mk
	echo "	touch /tmp/.world_done"		>> ${CHROOTDIR}/mk
	echo "fi"				>> ${CHROOTDIR}/mk
	echo "if [ -d /usr/src/release/sysinstall ]; then" >> ${CHROOTDIR}/mk
	echo "	cd /usr/src/release/sysinstall"	>> ${CHROOTDIR}/mk
	echo "	make obj"			>> ${CHROOTDIR}/mk
	echo "fi"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release"		>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make \$${_RELTARGET}"		>> ${CHROOTDIR}/mk
	echo "echo \">>> make ${.TARGET} for ${TARGET} finished on \`LC_ALL=C TZ=GMT date\`\"" >> ${CHROOTDIR}/mk
	chmod 755 ${CHROOTDIR}/mk
	env -i /usr/sbin/chroot ${CHROOTDIR} /mk

clean:
	rm -rf boot_crunch release.[0-9]

# Clean out ${_R} and make the directory structure.
release.1:
	mkdir -p ${_R}
	-rm -rf ${_R}/* 2> /dev/null
	-chflags -R noschg ${_R}/.
	rm -rf ${_R}/*
	mkdir ${RD}
	mkdir ${RD}/floppies
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
	touch release.1

# Install the system into the various distributions.
release.2:
	cd ${.CURDIR}/../etc && make distrib-dirs DESTDIR=${RD}/trees/bin
	cd ${.CURDIR}/.. && ${CROSSMAKE} distributeworld DISTDIR=${RD}/trees
	touch release.2

# Build and install crypto, krb4 and krb5 distributions.
release.3:
	# Handle some grief caused by the munition braindeadness.
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${WORLD_FLAGS} -DNO_MAKEDB_RUN -DNOCRYPT \
	    SUBDIR_OVERRIDE="${FIXCRYPTO}" \
	    buildworld distributeworld DISTDIR=${RD}/trees
.if exists(${.CURDIR}/../kerberosIV) && exists(${.CURDIR}/../crypto) && !defined(NOKERBEROS)
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${WORLD_FLAGS} -DNO_MAKEDB_RUN -DMAKE_KERBEROS4 \
	    SUBDIR_OVERRIDE="kerberosIV ${K4PROGS}" \
	    buildworld distributeworld DISTDIR=${RD}/trees
.endif
.if exists(${.CURDIR}/../kerberos5) && exists(${.CURDIR}/../crypto) && !defined(NOKERBEROS)
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${WORLD_FLAGS} -DNO_MAKEDB_RUN -DMAKE_KERBEROS5 \
	    SUBDIR_OVERRIDE="kerberos5 ${K5PROGS}" \
	    buildworld distributeworld DISTDIR=${RD}/trees
.endif
	-chflags -R noschg ${RD}/trees
	touch release.3

# Make and install the generic kernel(s).
release.4:
.for kernel in ${KERNELS}
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${KERNEL_FLAGS} -DNO_MODULES \
	    KERNCONF=${kernel} INSTKERNNAME=kernel.${kernel} kernel \
	    DESTDIR=${RD}/trees/bin
.endfor
	# Install a standard boot kernel.
	cd ${.CURDIR}/..; \
	${CROSSMAKE} ${KERNEL_FLAGS} \
	    INSTKERNNAME=kernel.GENERIC kernel \
	    DESTDIR=${RD}/trees/bin
	touch release.4

# Make and install the three crunched binaries which live on the floppies.
# You are not supposed to like this :-)
release.5:
	@mkdir -p /stand
	cd ${.CURDIR}/sysinstall && make obj depend all install
	rm -rf ${RD}/crunch
	mkdir -p ${RD}/crunch
.for j in ${CRUNCH_TARGETS}
	rm -rf ${j}_crunch
	mkdir ${j}_crunch
.if exists(${.CURDIR}/${TARGET}/${j}_crunch.conf)
	cd ${j}_crunch; ${WMAKEENV} crunchgen ${.CURDIR}/${TARGET}/${j}_crunch.conf
.else
	cd ${j}_crunch; ${WMAKEENV} crunchgen ${.CURDIR}/${j}_crunch.conf
.endif
	cd ${j}_crunch; ${WMAKE} -f ${j}_crunch.mk subclean
	cd ${.CURDIR}/..; ${TMAKE} build-tools
	cd ${j}_crunch; ${WMAKE} -f ${j}_crunch.mk all
	mv ${j}_crunch/${j}_crunch ${RD}/crunch/${j}
.endfor
	touch release.5

#
# --==## Fix up the distributions. ##==--
#
release.6:
	# Create any "synthetic dists" now.
	@for i in ${DISTRIBUTIONS}; do \
		if [ -f ${.CURDIR}/scripts/$${i}-make.sh ]; then \
			echo -n "Running $$i dist creation script... "; \
			env OBJFORMAT=${OBJFORMAT} RD=${RD} sh ${.CURDIR}/scripts/$${i}-make.sh || echo "$$i distribution script returned bad status."; \
			echo "Done."; \
		fi \
	done \

	# Remove all the directories we don't need.
	-cd ${RD}/trees && \
		find ${OTHER_DISTS} ${COMPAT_DISTS} ${CRYPTO_DISTS} -depth -type d -print | xargs rmdir
	touch release.6

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
	# More munition braindeadness.
	( cd ${RD}/dists && \
		if [ -f krb4/krb4.aa ] ; then \
			mv krb4/krb4.* crypto && \
			cat krb4/CHECKSUM.MD5 >> crypto/CHECKSUM.MD5 && \
			rm -r krb4; \
		fi )
	( cd ${RD}/dists && \
		if [ -f krb5/krb5.aa ] ; then \
			mv krb5/krb5.* crypto && \
			cat krb5/CHECKSUM.MD5 >> crypto/CHECKSUM.MD5 && \
			rm -r krb5; \
		fi )
	touch release.7


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
					-e 's/kerberosIV/krb4/' \
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
		if [ -f skrb4.aa ] ; then mv skrb4.* ../crypto ; fi && \
		if [ -f skrb5.aa ] ; then mv skrb5.* ../crypto ; fi ; \
		cd ${RD}/dists/crypto; rm -f CHECKSUM.MD5; \
		md5 * > CHECKSUM.MD5 ) ; fi
	(cd ${RD}/dists/src; rm -f CHECKSUM.MD5; md5 * > CHECKSUM.MD5)
	@echo "src distribution is finished."
.endif
	touch release.8

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
	-vnconfig 2>/dev/null
	rm -rf ${RD}/mfsfd
	mkdir ${RD}/mfsfd
	cd ${RD}/mfsfd && \
		mkdir -p etc/defaults dev mnt stand/help
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=boot \
		DIR=${RD}/mfsfd/stand ZIP=false
	( cd ${RD}/trees/bin/dev && \
		ls console tty bpf0 ttyd0 ttyv0 ttyv1 ttyv2 ttyv3 null zero card0 card1 card2 card3 usb usb0 uhid0 ums0 ulpt0 ugen0 kbd0 kmem mem xpt0 | \
	cpio -dump ${RD}/mfsfd/dev )
	( cd ${RD}/mfsfd/dev && rm -f *[swo]d*[bdefgh] )
	( cd ${RD}/mfsfd && \
	  for dir in bin sbin ; do \
		ln -sf /stand $$dir; \
	  done )
	cp ${RD}/trees/bin/sbin/dhclient-script ${RD}/mfsfd/stand
.if ${TARGET_ARCH} == "i386"
	cp ${.CURDIR}/../etc/defaults/pccard.conf ${RD}/mfsfd/etc/defaults/pccard.conf
.endif
	cp ${.CURDIR}/../etc/usbd.conf ${RD}/mfsfd/etc/usbd.conf
	cd ${RD}/trees/bin && ls ${BOOT1} | cpio -dump ${RD}/mfsfd/stand
	echo "nameserver      42/tcp name"	> ${RD}/mfsfd/stand/etc/services
	echo "ftp             21/tcp"		>> ${RD}/mfsfd/stand/etc/services
	echo "domain          53/tcp          nameserver" >> ${RD}/mfsfd/stand/etc/services
	echo "domain          53/udp          nameserver" >> ${RD}/mfsfd/stand/etc/services
	echo "cmd             514/tcp    shell" >> ${RD}/mfsfd/stand/etc/services
	gzip -9c ${RD}/trees/bin/COPYRIGHT > ${RD}/mfsfd/stand/help/COPYRIGHT.hlp.gz
.if !defined(NODOC)
	@for i in ${DIST_DOCS_ARCH_INDEP}; do \
	  gzip -9c ${RND}/${RELNOTES_LANG}/$$i/article.txt > ${RD}/mfsfd/stand/help/`echo $${i} | tr 'a-z' 'A-Z'`.TXT.gz; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  gzip -9c ${RND}/${RELNOTES_LANG}/$$i/${TARGET_ARCH}/article.txt > ${RD}/mfsfd/stand/help/`echo $${i} | tr 'a-z' 'A-Z'`.TXT.gz; \
	done
	@mv ${RD}/mfsfd/stand/help/INSTALLATION.TXT.gz ${RD}/mfsfd/stand/help/INSTALL.TXT.gz
.endif
	-test -f ${.CURDIR}/install.cfg && cp ${.CURDIR}/install.cfg ${RD}/mfsfd
	@mkdir -p ${RD}/mfsfd/boot
	@cp ${RD}/trees/bin/boot/boot* ${RD}/mfsfd/boot
.if ${TARGET} == "i386"
	@cp ${RD}/trees/bin/boot/mbr ${RD}/mfsfd/boot
.endif
	@cp ${RD}/trees/bin/boot/loader.help ${RD}/mfsfd/boot
	@echo "Making the regular boot floppy."
	@tar --exclude CVS -cf - -C ${.CURDIR}/sysinstall help | \
		tar xf - -C ${RD}/mfsfd/stand
	@echo "Compressing doc files..."
	@gzip -9 ${RD}/mfsfd/stand/help/*.hlp
.if exists(${.CURDIR}/${TARGET}/drivers.conf)
	@mkdir -p ${RD}/mfsfd/stand/modules
	@perl ${.CURDIR}/scripts/driver-copy2.pl \
		${.CURDIR}/${TARGET}/drivers.conf \
		${RD}/trees/bin/modules ${RD}/mfsfd/stand/modules
.endif
	sh -e ${.CURDIR}/scripts/doFS.sh -s mfsroot ${RD} ${MNT} \
		${MFSSIZE} ${RD}/mfsfd ${MFSINODE} ${MFSLABEL}
	@gzip -9vc mfsroot > mfsroot.gz
.if ${TARGET} == "pc98"
	@sh -e ${.CURDIR}/scripts/doFS.sh ${RD}/floppies/mfsroot.flp \
		${RD} ${MNT} ${SMALLBOOTSIZE} mfsroot.gz \
		${BOOTINODE} ${SMALLBOOTLABEL}
	@cd ${.CURDIR} && ${MAKE} doMFSKERN FSIMAGE=kern-small FDSIZE=SMALL
	@cd ${.CURDIR} && ${MAKE} doMFSKERN FSIMAGE=kern
.else
	@sh -e ${.CURDIR}/scripts/doFS.sh ${RD}/floppies/mfsroot.flp \
		${RD} ${MNT} ${BOOTSIZE} mfsroot.gz ${BOOTINODE} ${BOOTLABEL}
	@cd ${.CURDIR} && ${MAKE} doMFSKERN FSIMAGE=kern
	@cd ${.CURDIR} && ${MAKE} doMFSKERN FSIMAGE=boot FDSIZE=BIG
.endif
	@rm mfsroot mfsroot.gz
	@echo "Regular and MFS boot floppies made."
	touch release.9

#
# --==## Create a fixit floppy ##==--
#
release.10:
	@echo "Making fixit floppy."
	@rm -rf ${RD}/fixitfd
	@mkdir ${RD}/fixitfd
	@cd ${RD}/fixitfd && \
		mkdir -p dev stand bin sbin etc mnt mnt1 mnt2 mnt3 mnt4 tmp \
		usr/share/misc
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=fixit \
		DIR=${RD}/fixitfd/stand ZIP=false
	@( cd ${RD}/fixitfd/dev && \
		sed -e '/^PATH/s/^/#/' ${RD}/trees/bin/dev/MAKEDEV > MAKEDEV && \
		chmod 755 MAKEDEV && \
		sh MAKEDEV fixit )
	@cp ${RD}/trees/bin/etc/spwd.db ${RD}/trees/bin/etc/group \
		${RD}/trees/bin/etc/protocols ${RD}/fixitfd/etc
	@cp ${RD}/trees/bin/usr/share/misc/scsi_modes \
		${RD}/fixitfd/usr/share/misc
	@cp ${.CURDIR}/fixit.profile ${RD}/fixitfd/.profile
	@cp ${.CURDIR}/fixit.services ${RD}/fixitfd/etc/services
	@cp ${.CURDIR}/scripts/tar.sh ${RD}/fixitfd/stand/tar
	@chmod 555 ${RD}/fixitfd/stand/tar
	@sh -e ${.CURDIR}/scripts/doFS.sh ${RD}/floppies/fixit.flp ${RD} \
		${MNT} ${FIXITSIZE} ${RD}/fixitfd ${FIXITINODE} ${FIXITLABEL}
# Do our last minute floppies directory setup in a convenient place.
.if !defined(NODOC)
	@cp ${RND}/${RELNOTES_LANG}/installation/${TARGET_ARCH}/article.txt \
		${RD}/floppies/README.TXT
	@(cd ${RD}/floppies; md5 README.TXT *.flp > CHECKSUM.MD5)
.else
	@(cd ${RD}/floppies; md5 *.flp > CHECKSUM.MD5)
.endif
	touch release.10

#
# --==## Setup a suitable ftp-area ##==--
#
ftp.1:
	@echo "Setting up FTP distribution area"
	@mkdir -p ${FD}
	-@ln -s . ${FD}/${BUILDNAME}
	@cd ${RD} && find floppies -print | cpio -dumpl ${FD}
	@cd ${RD}/dists && find . -print | cpio -dumpl ${FD}
.if !defined(NODOC)
	@for i in ${DIST_DOCS_ARCH_INDEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.txt ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${RND}/${RELNOTES_LANG}/$$i/article.html ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET_ARCH}/article.txt ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET_ARCH}/article.html ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@cp ${RND}/${RELNOTES_LANG}/readme/docbook.css ${FD}
	@mv ${FD}/INSTALLATION.TXT ${FD}/INSTALL.TXT
	@mv ${FD}/INSTALLATION.HTM ${FD}/INSTALL.HTM
.endif
	@echo "CD_VERSION = ${BUILDNAME}" > ${FD}/cdrom.inf
.if !defined(NOPORTS)
	@tar --exclude CVS -cBf - -C ${CD_DISC1} ports | tar -xBf - -C ${FD}
.endif
	touch ftp.1

#
# --==## Setup a suitable cdrom-area ##==--
#
cdrom.1:
	@echo "Setting up CDROM distribution area"
	@mkdir -p ${CD_DISC1} ${CD_DISC2}
	@cd ${RD} && find floppies -print | cpio -dumpl ${CD_DISC1}
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
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET_ARCH}/article.txt ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${RND}/${RELNOTES_LANG}/$$i/${TARGET_ARCH}/article.html ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@cp ${RND}/${RELNOTES_LANG}/readme/docbook.css ${CD_DISC1}
	@mv ${CD_DISC1}/INSTALLATION.TXT ${CD_DISC1}/INSTALL.TXT
	@mv ${CD_DISC1}/INSTALLATION.HTM ${CD_DISC1}/INSTALL.HTM
.endif
.if ${TARGET} != "pc98"
	@echo "Setting up /boot"
	@cp -Rp ${CD_DISC2}/boot ${CD_DISC1}
	@ln -f ${RD}/image.boot/mfsroot.gz ${CD_DISC1}/boot/mfsroot.gz
	@ln -f ${CD_DISC1}/boot/mfsroot.gz ${CD_DISC2}/boot/mfsroot.gz
	@echo 'mfsroot_load="YES"' > ${CD_DISC1}/boot/loader.conf
	@echo 'mfsroot_type="mfs_root"' >> ${CD_DISC1}/boot/loader.conf
	@echo 'mfsroot_name="/boot/mfsroot"' >> ${CD_DISC1}/boot/loader.conf
.if ${TARGET} == "i386"
	@echo 'userconfig_script_load="YES"' >> ${CD_DISC1}/boot/loader.conf
	@echo 'intro' > ${CD_DISC1}/boot/kernel.conf
.endif
	@ln -f ${CD_DISC1}/boot/loader.conf ${CD_DISC2}/boot/loader.conf
	@ln -f ${CD_DISC2}/kernel.GENERIC ${CD_DISC1}/kernel
.endif
.if ${TARGET} == "i386"
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
	touch cdrom.1

iso.1:
	@if [ -r ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh ]; then \
		echo "Creating ISO images..."; \
		sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh ${BOOTABLE} \
		  fbsd_miniinst ${CD}/miniinst.iso ${CD_DISC1}; \
		sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh ${BOOTABLE} \
		  fbsd_livefs ${CD}/disc2.iso ${CD_DISC2}; \
		if [ "x${CD_EXTRA_BITS}" != "x" ]; then \
			sh ${.CURDIR}/${TARGET_ARCH}/mkisoimages.sh ${BOOTABLE} \
			  fbsd_boot ${CD}/disc1.iso ${CD_DISC1} ${CD_EXTRA_BITS}; \
		fi \
	else \
		echo "Do not know how to create an ISO for ${TARGET_ARCH}."; \
	fi
	touch iso.1

#
# --==## Documentation Project files such as the Handbook and FAQ ##==--
#
doc.1:
	@echo "Making docs..."
	@for i in ${DOCPORTS}; do \
	  cd /usr/ports/$$i && make all install clean BATCH=yes JADETEX=no WITHOUT_PYTHON=yes FORCE_PKG_REGISTER=yes; \
	done
	@cd /usr/doc && make all install 'FORMATS=html html-split txt' INSTALL_COMPRESSED='' DOCDIR=${RD}/trees/bin/usr/share/doc
	touch doc.1

#
# --==## RELNOTESng:  Next-generation replacements for *.TXT files ##==--
#
doc.2:
	@echo "Making release documentation..."
	@cd ${.CURDIR}/doc && make all install clean 'FORMATS=html txt' INSTALL_COMPRESSED='' DOCDIR=${RND}
	touch doc.2

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
	@cd ${.CURDIR} && ${MAKE} boot.flp
	@cd ${.CURDIR} && ${MAKE} fixit.flp
	@cd ${RD} && find floppies -print | cpio -dumpl ${FD}

boot.flp:
	@rm -f release.5 release.9
	@cd ${.CURDIR} && ${MAKE} release.5 release.9 CRUNCH_TARGETS=boot

fixit.flp:
	@rm -f release.5 release.10
	@cd ${.CURDIR} && ${MAKE} release.5 release.10 CRUNCH_TARGETS=fixit

write_mfs_in_kernel:	${.CURDIR}/write_mfs_in_kernel.c
	${CC} ${CFLAGS} -o write_mfs_in_kernel ${.CURDIR}/write_mfs_in_kernel.c

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
	@cd ${.CURDIR}/../sys/${TARGET}/conf && \
	    sh ${.CURDIR}/scripts/dokern.sh ${FDSIZE} < GENERIC > BOOTMFS
.if ${TARGET_ARCH} == "i386"
	@echo "options  INTRO_USERCONFIG" >> \
	  ${.CURDIR}/../sys/${TARGET}/conf/BOOTMFS
.endif
.if exists(${.CURDIR}/${TARGET}/drivers.conf)
	@perl ${.CURDIR}/scripts/driver-remove.pl \
		${.CURDIR}/${TARGET}/drivers.conf \
		${.CURDIR}/../sys/${TARGET}/conf/BOOTMFS
.endif
	cd ${.CURDIR}/..; \
	INSTKERNNAME=BOOTMFS \
	    ${CROSSMAKE} ${KERNEL_FLAGS} -DNO_MODULES -DNO_KERNELCLEAN \
	    KERNCONF=BOOTMFS buildkernel reinstallkernel \
	    DESTDIR=${RD}/kernels
	@rm -rf ${RD}/image.${FSIMAGE}
	@mkdir ${RD}/image.${FSIMAGE}
	@cd ${RD}/kernels && \
	  (chflags noschg BOOTMFS || true) && \
	  ${WMAKEENV} strip BOOTMFS && \
	  cp BOOTMFS BOOTMFS.${FSIMAGE}
	mv ${RD}/kernels/BOOTMFS ${RD}/image.${FSIMAGE}/kernel
	@echo "Setting up /boot directory for ${FSIMAGE} floppy"
	@mkdir -p ${RD}/image.${FSIMAGE}/boot
.if ${TARGET} == "i386"
	@kgzip -vo ${RD}/image.${FSIMAGE}/boot/loader /boot/loader
.else
	@cp /boot/loader ${RD}/image.${FSIMAGE}/boot
.endif
	@echo "load /kernel" > ${RD}/image.${FSIMAGE}/boot/loader.rc
.if !defined(FDSIZE) || ${FDSIZE} != "BIG"
	@echo "echo \\007\\007" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "echo Please insert MFS root floppy and press enter:" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "read" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.endif
	@echo "load -t mfs_root /mfsroot" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "echo \\007\\007" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "autoboot 10" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.if ${TARGET_ARCH} == "i386" && ${AUTO_KEYBOARD_DETECT}
	@echo "-P" >> ${RD}/image.${FSIMAGE}/boot.config
.endif
	@gzip -9v ${RD}/image.${FSIMAGE}/kernel
	@rm -f ${RD}/floppies/${FSIMAGE}.flp
.if defined(FDSIZE) && ${FDSIZE} == "BIG"
	@cp mfsroot.gz ${RD}/image.${FSIMAGE}
	sh -e ${.CURDIR}/scripts/doFS.sh ${RD}/floppies/${FSIMAGE}.flp \
		${RD} ${MNT} ${BIGBOOTSIZE} ${RD}/image.${FSIMAGE} \
		${BOOTINODE} ${BIGBOOTLABEL}
.elif defined(FDSIZE) && ${FDSIZE} == "SMALL"
	sh -e ${.CURDIR}/scripts/doFS.sh ${RD}/floppies/${FSIMAGE}.flp \
		${RD} ${MNT} ${SMALLBOOTSIZE} ${RD}/image.${FSIMAGE} \
		${BOOTINODE} ${SMALLBOOTLABEL}
.else
	sh -e ${.CURDIR}/scripts/doFS.sh ${RD}/floppies/${FSIMAGE}.flp \
		${RD} ${MNT} ${BOOTSIZE} ${RD}/image.${FSIMAGE} \
		${BOOTINODE} ${BOOTLABEL}
.endif
	@echo "Created ${RD}/floppies/${FSIMAGE}.flp"

.include <bsd.prog.mk>
