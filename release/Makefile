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
# Please note: the md(4) driver must be compiled into your kernel
# or available as a kld(4)-style kernel module,
# otherwise the target 'release.8' and possibly others will fail.
#
# Note:  "/some/dir" cannot reside on a filesystem mounted with
# the "nodev" option, otherwise the chrooted "buildworld" will likely
# fail.
#
# Set these, release builder!
#
# Fixed version:
#BUILDNAME=5.0-RELEASE
#
# Automatic SNAP versioning:
DATE != date +%Y%m%d
BASE = 5.0
BUILDNAME?=${BASE}-${DATE}-SNAP
#
#CHROOTDIR=/junk/release
# If this is a -stable snapshot, then set
#RELEASETAG=RELENG_4
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

KERNCONF=GENERIC

# If you want to pass flags to the world build such as -j X, use
# WORLD_FLAGS.  Similarly, you can specify make flags for kernel
# builds via KERNEL_FLAGS.
#WORLD_FLAGS=-j4
#KERNEL_FLAGS=-j4

# If you are using a local CVS repository with components stored in 
# non-standard modules, override these on the make commandline or
# in the environment.
RELEASESRCMODULE?=	src
RELEASEDOCMODULE?=	doc
RELEASEPORTSMODULE?=	ports

# Unless set elsewhere, indicate the object format we'll be using.
OBJFORMAT?=		elf

# Uncomment this to disable the doc.1 target.  Docs normally require
# the ports tree, so NOPORTS can be set together with NODOC in order
# to have neither ports or docs.  If only NOPORTS is set to YES, but
# docs are still desired, the DOMINIMALDOCPORTS logic below will only
# install the ports that are minimally required for the docs.  This is
# intended as a compromise, less disk space is required than for using
# the entire ports collection (und much less time due to the huge number
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
# Set this to wherever the distfiles required by ${DOCPORTS} live.
DOCDISTFILES?=	${.CURDIR}/../../ports/distfiles
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
.if ${MACHINE_ARCH} == "i386"
COMPAT_DISTS?=	compat1x compat20 compat21 compat22 compat3x compat4x
.else
COMPAT_DISTS?=	compat4x
.endif
OTHER_DISTS?=	manpages catpages games proflibs dict info doc
CRYPTO_DISTS?=	crypto krb4 krb5
BIN_DISTS?=	bin
DISTRIBUTIONS?=	${BIN_DISTS} ${OTHER_DISTS} ${COMPAT_DISTS} ${CRYPTO_DISTS}
KERNELS?=	GENERIC

BOOT1=	etc/defaults/rc.conf

# mountpoint for filesystems.
MNT=			/mnt

# Various floppy image parameters.
#

.if ${MACHINE_ARCH} == "i386"
.if ${MACHINE} == "pc98"
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
.elif ${MACHINE_ARCH} == "alpha"
BOOTSIZE=		1440
FIXITSIZE=		2880
MFSSIZE=		3200
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

# Things that need to be compiled without crypto support in releases
.if !defined(FIXCRYPTO)
FIXCRYPTO=	bin/ed usr.sbin/ppp usr.sbin/pppd usr.sbin/tcpdump/tcpdump 
.if !defined(NO_SENDMAIL)
FIXCRYPTO+=	usr.sbin/sendmail
.endif
.endif


# Things which may get you into trouble if you change them
MTREEFILES=		${.CURDIR}/../etc/mtree
_R?=			/R
RD=			${_R}/stage
FD=			${_R}/ftp
CD=			${_R}/cdrom
CD_DISC1=		${CD}/disc1
CD_DISC2=		${CD}/disc2

# Where the bootstrap ports (see DOCPORTS) get installed.
LOCALDIR=		/usr/local/bin

# ${BOOTSTRAPDIR} is for those utilities that refer to the hosting
# environment, rather than the target environment.  This is specifically
# intended for kernel-dependent utilities that are used during the build.
#
# ${BOOTSTRAPDIR} is actually being used by prepending it to the normal
# ${PATH}.  Thus, it's also available to outside utilities like doFS.sh.
BOOTSTRAPDIR=	/bootstrap
#
# The mount subsystem has been changed between 2.2 and 3.0 by the
# Lite2 import.
BOOTSTRAPUTILS=	/sbin/mount /sbin/umount
#
# 3.0 cpio tries to reference lchown(2) which is not available in 2.2
BOOTSTRAPUTILS+= /usr/bin/cpio

.if !defined(CRUNCH_TARGETS)
CRUNCH_TARGETS=	boot fixit
.endif

EXTRAS= cdrom.1 ftp.1
.if defined(MAKE_ISOS)
EXTRAS+= iso.1
.endif

.if !defined(NODOC)
DOCREL= doc.1 doc.2
.endif

.if !defined(NOPORTREADMES)
MAKEREADMES=	make readmes PORTSDIR=${CHROOTDIR}/usr/ports
.else
MAKEREADMES=	true
.endif

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
	@echo ">>> make release started on `LC_ALL=C TZ=GMT date`"
	cd ${.CURDIR}/../etc && ${MAKE} distrib-dirs DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/../etc && ${MAKE} distribution DESTDIR=${CHROOTDIR}
	if [ -f /etc/resolv.conf ]; then \
		cp -p /etc/resolv.conf ${CHROOTDIR}/etc; \
	fi
	cd ${.CURDIR}/.. && ${MAKE} installworld DESTDIR=${CHROOTDIR} NOMAN=1
	mkdir ${CHROOTDIR}/${BOOTSTRAPDIR}
	for i in ${BOOTSTRAPUTILS} ; do \
		cp -p ${CHROOTDIR}$$i ${CHROOTDIR}/${BOOTSTRAPDIR} ; \
	done
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf src && \
		cvs -R -d ${CVSROOT} co -P ${RELEASESRCMODULE}
.else
	cd ${CHROOTDIR}/usr && rm -rf src && \
		cvs -R -d ${CVSROOT} co -P -r ${RELEASETAG} ${RELEASESRCMODULE}
.endif
.if defined(LOCAL_PATCHES) && exists(${LOCAL_PATCHES})
	cd ${CHROOTDIR}/usr/src && patch ${PATCH_FLAGS} < ${LOCAL_PATCHES}
.endif
.if defined(LOCAL_SCRIPT) && exists(${LOCAL_SCRIPT})
	cd ${CHROOTDIR} && env CHROOTDIR=${CHROOTDIR} BUILDNAME=${BUILDNAME} RELEASETAG=${RELEASETAG} ${LOCAL_SCRIPT}
.endif
.if !defined(NOPORTS)
.if defined(PORTSRELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co -P -r ${PORTSRELEASETAG} ${RELEASEPORTSMODULE} && cd ports && ${MAKEREADMES}
.else
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co -P ${RELEASEPORTSMODULE} && cd ports && ${MAKEREADMES}
.endif
.elif defined(DOMINIMALDOCPORTS) && ${DOMINIMALDOCPORTS} == "YES"
.if defined(PORTSRELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co -P -r ${PORTSRELEASETAG} ${MINIMALDOCPORTS}
.else
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co -P ${MINIMALDOCPORTS}
.endif
.endif
.if !defined(NODOC)
.if defined(DOCRELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf doc && cvs -R -d ${CVSROOT} co -P -r ${DOCRELEASETAG} ${RELEASEDOCMODULE}
.else
	cd ${CHROOTDIR}/usr && rm -rf doc && cvs -R -d ${CVSROOT} co -P ${RELEASEDOCMODULE}
.endif
	if [ -d ${DOCDISTFILES}/ ]; then \
		cp -rp ${DOCDISTFILES} ${CHROOTDIR}/usr/ports/distfiles; \
	fi
.endif
.endif
.if make(rerelease)
.if !defined(RELEASENOUPDATE)
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr/src && cvs -R -q update -P -d
.else
	cd ${CHROOTDIR}/usr/src && cvs -R -q update -P -d -r ${RELEASETAG}
.endif
.if !defined(NOPORTS)
	cd ${CHROOTDIR}/usr/ports && cvs -R -q update -P -d
.endif
.if defined(DOMINIMALDOCPORTS) && ${DOMINIMALDOCPORTS} == "YES"
	for i in ${MINIMALDOCPORTS}; do \
		( cd ${CHROOTDIR}/usr/$$i && cvs -R -q update -P -d ) ; \
	done
.endif
.if !defined(NODOC)
	cd ${CHROOTDIR}/usr/doc && cvs -R -q update -P -d
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
	echo "export CFLAGS='-O -pipe'"	>> ${CHROOTDIR}/mk
	echo "export WITHOUT_X11=YES"		>> ${CHROOTDIR}/mk
	echo "export DISTRIBUTIONS=\"${DISTRIBUTIONS}\"" >> ${CHROOTDIR}/mk
	echo "export BUILDNAME=${BUILDNAME}"	>> ${CHROOTDIR}/mk
	echo "export OBJFORMAT=${OBJFORMAT}"	>> ${CHROOTDIR}/mk
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
.if defined(KERNEL_FLAGS)
	echo "export KERNEL_FLAGS=\"${KERNEL_FLAGS}\"" >> ${CHROOTDIR}/mk
.endif
	# Don't remove this, or the build will fall over!
	echo "export RELEASEDIR=${_R}"		>> ${CHROOTDIR}/mk
	echo "export PATH=${BOOTSTRAPDIR}:$${PATH}:${LOCALDIR}" >> ${CHROOTDIR}/mk
	echo "if [ ! -f /tmp/.world_done ]; then" >> ${CHROOTDIR}/mk
	echo "	cd /usr/src"			>> ${CHROOTDIR}/mk
.if make(release)
	echo "	(cd etc; make distrib-dirs distribution)" >> ${CHROOTDIR}/mk
	echo "	make ${WORLD_FLAGS} world && \\"	>> ${CHROOTDIR}/mk
.endif
.if make(rerelease)
	echo "	make ${WORLD_FLAGS} all install && \\" >> ${CHROOTDIR}/mk
.endif
	echo "	touch /tmp/.world_done"		>> ${CHROOTDIR}/mk
	echo "fi"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release"		>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make \$${_RELTARGET}"		>> ${CHROOTDIR}/mk
	echo "echo \">>> make ${.TARGET} finished on \`LC_ALL=C TZ=GMT date\`\"" >> ${CHROOTDIR}/mk
	chmod 755 ${CHROOTDIR}/mk
	chroot ${CHROOTDIR} /mk

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
	mkdir ${RD}/dists
	mkdir ${RD}/kernels
	for i in ${DISTRIBUTIONS} ; do \
		mkdir ${RD}/trees/$$i && \
		mkdir ${RD}/dists/$$i && \
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
	cd ${.CURDIR}/.. && make distribworld DISTDIR=${RD}/trees
.if exists(${.CURDIR}/../kerberosIV) && exists(${.CURDIR}/../crypto) && !defined(NOKERBEROS)
	cd ${.CURDIR}/../kerberosIV && ( \
		make bootstrap &&\
		make obj all help-distribute DISTDIR=${RD}/trees &&\
		make kprog \
	)
.endif
.if exists(${.CURDIR}/../kerberos5) && exists(${.CURDIR}/../crypto) && !defined(NOKERBEROS)
	cd ${.CURDIR}/../kerberos5 && ( \
		make bootstrap &&\
		make obj all help-distribute DISTDIR=${RD}/trees &&\
		make kprog \
	)
.endif
	-chflags -R noschg ${RD}/trees
	touch release.2

# Make and install the generic kernel(s).
release.3:
.for kernel in ${KERNELS}
	-chflags -R noschg ${RD}/kernels/${kernel}
	rm -rf ${RD}/kernels/${kernel}
	rm -rf ${.CURDIR}/../sys/${MACHINE}/compile/${kernel}
	cd ${.CURDIR} && ${MAKE} doSTDKERNEL KERNEL=${kernel} KODIR=/${kernel}
	rm -rf ${.CURDIR}/../sys/${MACHINE}/compile/${kernel}
	-mkdir ${RD}/trees/bin/boot/${kernel}
	cp -p ${RD}/kernels/${kernel}/kernel ${RD}/trees/bin/boot/${kernel}
.endfor
	# Install a standard boot kernel+modules
	mkdir -p ${RD}/trees/bin/boot/kernel
	cp -Rp ${RD}/kernels/GENERIC/* ${RD}/trees/bin/boot/kernel
	touch release.3

# Make and install the three crunched binaries which live on the floppies.
# You are not supposed to like this :-)
release.4:
	@mkdir -p /stand
	rm -rf ${RD}/crunch
	mkdir -p ${RD}/crunch
	for j in ${CRUNCH_TARGETS} ; do \
		rm -rf $${j}_crunch && \
		mkdir $${j}_crunch && \
		( cd $${j}_crunch && \
		  ( ( [ -f ${.CURDIR}/${MACHINE}/$${j}_crunch.conf ] && \
		    crunchgen ${.CURDIR}/${MACHINE}/$${j}_crunch.conf ) || \
		    ( crunchgen ${.CURDIR}/$${j}_crunch.conf ) ) && \
		  ${MAKE} -f $${j}_crunch.mk subclean all ) && \
		mv $${j}_crunch/$${j}_crunch ${RD}/crunch/$${j} && \
		true || { rm -rf $${j}_crunch ; false ; } ; \
	done
	touch release.4

#
# --==## Fix up the distributions. ##==--
#
release.5:
	# Handle some grief caused by the munition braindeadness.
	for i in ${FIXCRYPTO}; do \
		( cd ${.CURDIR}/../$$i; \
		  make -DNOCRYPT clean all distribute DISTDIR=${RD}/trees ) ; \
	done

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
	touch release.5

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
	touch release.6


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
	touch release.7

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

release.8:
	rm -rf ${RD}/mfsfd
	mkdir ${RD}/mfsfd
	cd ${RD}/mfsfd && \
		mkdir -p etc/defaults dev mnt stand/help
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=boot \
		DIR=${RD}/mfsfd/stand ZIP=false
	( cd ${RD}/trees/bin/dev && \
		ls console tty bpf0 ttyd0 ttyv0 ttyv1 ttyv2 ttyv3 null zero card0 card1 card2 card3 usb usb0 uhid0 ums0 ulpt0 ugen0 kbd0 kmem mem | \
	cpio -dump ${RD}/mfsfd/dev )
	( cd ${RD}/mfsfd/dev && rm -f *[swo]d*[bdefgh] )
	( cd ${RD}/mfsfd && \
	  for dir in bin sbin ; do \
		ln -sf /stand $$dir; \
	  done )
	cp /sbin/dhclient-script ${RD}/mfsfd/stand
	cp ${.CURDIR}/../etc/defaults/pccard.conf ${RD}/mfsfd/etc/defaults/pccard.conf
	cp ${.CURDIR}/../etc/usbd.conf ${RD}/mfsfd/etc/usbd.conf
	cd ${RD}/trees/bin && ls ${BOOT1} | cpio -dump ${RD}/mfsfd/stand
	( for F in netconfig protocols ; do \
		sed -e '/^#.*$$/d' -e 's/[:space:]*#.*$$//g' \
		${RD}/trees/bin/etc/$$F > ${RD}/mfsfd/stand/etc/$$F ; \
	  done )
	grep -E '^(ftp|nameserver|domain|sunrpc|cmd|nfsd)[^-\w]' \
	    ${.CURDIR}/../etc/services | \
	    sed -e '/^#.*$$/d' -e 's/[:space:]*#.*$$//g' \
	    > ${RD}/mfsfd/stand/etc/services
	ln ${RD}/mfsfd/stand/etc/services ${RD}/mfsfd/etc/services
	ln ${RD}/mfsfd/stand/etc/netconfig ${RD}/mfsfd/etc/netconfig
	gzip -9c ${.CURDIR}/../COPYRIGHT > ${RD}/mfsfd/stand/help/COPYRIGHT.hlp.gz
.if !defined(NODOC)
	@for i in ${DIST_DOCS_ARCH_INDEP}; do \
	  gzip -9c ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/article.txt > ${RD}/mfsfd/stand/help/`echo $${i} | tr 'a-z' 'A-Z'`.TXT.gz; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  gzip -9c ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/${MACHINE_ARCH}/article.txt > ${RD}/mfsfd/stand/help/`echo $${i} | tr 'a-z' 'A-Z'`.TXT.gz; \
	done
	@mv ${RD}/mfsfd/stand/help/INSTALLATION.TXT.gz ${RD}/mfsfd/stand/help/INSTALL.TXT.gz
.endif
	-test -f ${.CURDIR}/install.cfg && cp ${.CURDIR}/install.cfg ${RD}/mfsfd
	@mkdir -p ${RD}/mfsfd/boot
	@cp /boot/boot* ${RD}/mfsfd/boot
	@cp /boot/loader.help ${RD}/mfsfd/boot
	@cd ${.CURDIR} && ${MAKE} createBOOTMFS 
.if exists(${.CURDIR}/${MACHINE_ARCH}/drivers.conf)
	@cd ${.CURDIR} && ${MAKE} doMODULES KERNEL=BOOTMFS KERNEL_KO=BOOTMFS KODIR=""
.endif
	@echo "Making the regular boot floppy."
	@tar --exclude CVS -cf - -C /usr/src/usr.sbin/sysinstall help | \
		tar xf - -C ${RD}/mfsfd/stand
	@echo "Compressing doc files..."
	@gzip -9 ${RD}/mfsfd/stand/help/*.hlp
.if ${MACHINE_ARCH} == "alpha"
	rm -rf ${RD}/mfsfd/stand/help/*
.endif
.if exists(${.CURDIR}/${MACHINE_ARCH}/drivers.conf)
	@mkdir -p ${RD}/mfsfd/stand/modules
	@perl ${.CURDIR}/scripts/driver-copy2.pl \
		${.CURDIR}/${MACHINE_ARCH}/drivers.conf \
		${RD}/kernels ${RD}/mfsfd/stand/modules
.endif
	sh -e ${.CURDIR}/scripts/doFS.sh -s mfsroot ${RD} ${MNT} \
		${MFSSIZE} ${RD}/mfsfd ${MFSINODE} ${MFSLABEL}
	@gzip -9vc mfsroot > mfsroot.gz
.if ${MACHINE} == "pc98"
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
	touch release.8

#
# --==## Create a fixit floppy ##==--
#
release.9:
	@echo "Making fixit floppy."
	@rm -rf ${RD}/fixitfd
	@mkdir ${RD}/fixitfd
	@cd ${RD}/fixitfd && \
		mkdir -p dev stand bin sbin etc mnt mnt1 mnt2 mnt3 mnt4 tmp \
		usr/share/misc
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=fixit \
		DIR=${RD}/fixitfd/stand ZIP=false
	@( cd ${RD}/fixitfd/dev && \
		cp ${RD}/trees/bin/dev/MAKEDEV MAKEDEV && \
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
	@cp ${.CURDIR}/doc/${RELNOTES_LANG}/installation/${MACHINE_ARCH}/article.txt \
		${RD}/floppies/README.TXT
	@(cd ${RD}/floppies; md5 README.TXT *.flp > CHECKSUM.MD5)
.else
	@(cd ${RD}/floppies; md5 *.flp > CHECKSUM.MD5)
.endif
	touch release.9

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
	  cp ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/article.txt ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/article.html ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  cp ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/${MACHINE_ARCH}/article.txt ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/${MACHINE_ARCH}/article.html ${FD}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@cp ${.CURDIR}/doc/${RELNOTES_LANG}/readme/docbook.css ${FD}
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
	  cp ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/article.txt ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/article.html ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@for i in ${DIST_DOCS_ARCH_DEP}; do \
	  cp ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/${MACHINE_ARCH}/article.txt ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.TXT; \
	  cp ${.CURDIR}/doc/${RELNOTES_LANG}/$$i/${MACHINE_ARCH}/article.html ${CD_DISC1}/`echo $${i} | tr 'a-z' 'A-Z'`.HTM; \
	done
	@cp ${.CURDIR}/doc/${RELNOTES_LANG}/readme/docbook.css ${CD_DISC1}
	@mv ${CD_DISC1}/INSTALLATION.TXT ${CD_DISC1}/INSTALL.TXT
	@mv ${CD_DISC1}/INSTALLATION.HTM ${CD_DISC1}/INSTALL.HTM
.endif
	@echo "Setting up /boot"
	@cp -Rp ${CD_DISC2}/boot ${CD_DISC1}
	@ln -f ${RD}/image.boot/mfsroot.gz ${CD_DISC1}/boot/mfsroot.gz
	@ln -f ${CD_DISC1}/boot/mfsroot.gz ${CD_DISC2}/boot/mfsroot.gz
	@echo 'mfsroot_load="YES"' > ${CD_DISC1}/boot/loader.conf
	@echo 'mfsroot_type="mfs_root"' >> ${CD_DISC1}/boot/loader.conf
	@echo 'mfsroot_name="/boot/mfsroot"' >> ${CD_DISC1}/boot/loader.conf
	@ln -f ${CD_DISC1}/boot/loader.conf ${CD_DISC2}/boot/loader.conf
.if ${MACHINE} == "i386"
	@mkdir -p ${CD_DISC2}/floppies
	@cp ${CD_DISC1}/floppies/boot.flp ${CD_DISC2}/floppies
.endif
.if !defined(NOPORTS)
	@-rm -rf /usr/ports/distfiles/*
	@mkdir -p ${CD_DISC1}/ports && \
	  tar --exclude CVS -czf ${CD_DISC1}/ports/ports.tgz -C /usr ports && \
	  cp ${.CURDIR}/scripts/ports-install.sh ${CD_DISC1}/ports/install.sh \
	  && (cd ${CD_DISC1}/ports; md5 * > CHECKSUM.MD5)
.endif
	touch cdrom.1

iso.1:
	@if [ -r ${.CURDIR}/${MACHINE_ARCH}/mkisoimages.sh ]; then \
		echo "Creating ISO images..."; \
		sh ${.CURDIR}/${MACHINE_ARCH}/mkisoimages.sh -b fbsd_miniinst \
		  ${CD}/miniinst.iso ${CD_DISC1}; \
		sh ${.CURDIR}/${MACHINE_ARCH}/mkisoimages.sh -b fbsd_livefs \
		  ${CD}/disc2.iso ${CD_DISC2}; \
		if [ "x${CD_EXTRA_BITS}" != "x" ]; then \
			sh ${.CURDIR}/${MACHINE_ARCH}/mkisoimages.sh -b fbsd_boot \
			  ${CD}/disc1.iso ${CD_DISC1} ${CD_EXTRA_BITS}; \
			    && false; \
		fi \
	else \
		echo "Do not know how to create an ISO for ${MACHINE_ARCH}."; \
	fi
	touch iso.1

#
# --==## Documentation Project files such as the Handbook and FAQ ##==--
#
doc.1:
	@echo "Making docs..."
	@for i in ${DOCPORTS}; do \
	  cd /usr/ports/$$i && make all install clean BATCH=yes JADETEX=no FORCE_PKG_REGISTER=yes; \
	done
	@cd /usr/doc && make all install 'FORMATS=html html-split txt' INSTALL_COMPRESSED='' DOCDIR=${RD}/trees/bin/usr/share/doc
	touch doc.1

#
# --==## RELNOTESng:  Next-generation replacements for *.TXT files ##==--
#
doc.2:
	@echo "Making release documentation..."
	@cd ${.CURDIR}/doc && make all 'FORMATS=html txt'
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

doRELEASE:  release.1 release.2 ${DOCREL} release.3 release.4 release.5 \
		release.6 release.7 release.8 release.9
	@cd ${.CURDIR} && ${MAKE} ${EXTRAS}
	@echo "Release done"

floppies:
	@cd ${.CURDIR} && ${MAKE} boot.flp
	@cd ${.CURDIR} && ${MAKE} fixit.flp
	@cd ${RD} && find floppies -print | cpio -dumpl ${FD}

boot.flp:
	@rm -f release.4 release.8
	@cd ${.CURDIR} && ${MAKE} release.4 release.8 CRUNCH_TARGETS=boot

fixit.flp:
	@rm -f release.4 release.9
	@cd ${.CURDIR} && ${MAKE} release.4 release.9 CRUNCH_TARGETS=fixit

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
	@if [ -f ${.CURDIR}/${MACHINE}/${CRUNCH}_crunch.conf ] ; then \
		for i in `crunchgen -l ${.CURDIR}/${MACHINE}/${CRUNCH}_crunch.conf` ; do \
			ln -f ${DIR}/${CRUNCH}_crunch ${DIR}/$$i ; \
		done \
	else \
		for i in `crunchgen -l ${.CURDIR}/${CRUNCH}_crunch.conf` ; do \
			ln -f ${DIR}/${CRUNCH}_crunch ${DIR}/$$i ; \
		done \
	fi

#
# --==## BOOTMFS config file ##==--
#

createBOOTMFS:
	@cd ${.CURDIR}/../sys/${MACHINE}/conf && \
	  sh ${.CURDIR}/${MACHINE_ARCH}/dokern.sh ${FDSIZE} < ${KERNCONF} > BOOTMFS && \
	  [ -r ${KERNCONF}.hints ] && cp ${KERNCONF}.hints BOOTMFS.hints
.if 0 && ${MACHINE_ARCH} == "i386"
	@echo "options  INTRO_USERCONFIG" >> \
	  ${.CURDIR}/../sys/${MACHINE}/conf/BOOTMFS
.endif
.if exists(${.CURDIR}/${MACHINE_ARCH}/drivers.conf)
	@perl ${.CURDIR}/scripts/driver-remove.pl \
		${.CURDIR}/${MACHINE_ARCH}/drivers.conf \
		${.CURDIR}/../sys/${MACHINE}/conf/BOOTMFS
.endif

#
# --==## Compile a kernel by name ${KERNEL} ##==--
#
# We don't erase the sys/${MACHINE}/compile/${KERNEL} directory, since somebody
# may want to reuse it (release.8 presently)
#
doKERNEL:
	@rm -f ${RD}/kernels/${KERNEL}
	@cd ${.CURDIR}/../sys/${MACHINE}/conf && config ${KERNEL}
	@cd ${.CURDIR}/../sys/${MACHINE}/compile/${KERNEL} && \
			make kernel-depend && \
			make ${KERNEL_FLAGS} ${KERNEL_KO} && \
			make kernel-reinstall DESTDIR=${RD}/kernels && \
			[ -r ${.CURDIR}/../sys/${MACHINE}/conf/${KERNEL}.hints ] && \
			cp ${.CURDIR}/../sys/${MACHINE}/conf/${KERNEL}.hints ${RD}/kernels

doMODULES:
	@rm -f ${RD}/kernels/*.ko
	@cd ${.CURDIR}/../sys/${MACHINE}/conf && config ${KERNEL}
	@cd ${.CURDIR}/../sys/${MACHINE}/compile/${KERNEL} && \
			make modules-depend && \
			make ${KERNEL_FLAGS} modules && \
			make modules-reinstall DESTDIR=${RD}/kernels && \

doSTDKERNEL:
	@rm -f ${RD}/kernels/${KERNEL}
	@cd ${.CURDIR}/../sys/${MACHINE}/conf && config ${KERNEL}
	@cd ${.CURDIR}/../sys/${MACHINE}/compile/${KERNEL} && \
			make depend && \
			make ${KERNEL_FLAGS} KERNEL=${KERNEL} && \
			make KERNEL=${KERNEL} DESTDIR=${RD}/kernels install && \
			[ -r ${.CURDIR}/../sys/${MACHINE}/conf/${KERNEL}.hints ] && \
			cp ${.CURDIR}/../sys/${MACHINE}/conf/${KERNEL}.hints ${RD}/kernels
	@-cd ${.CURDIR}/../sys/${MACHINE}/compile/${KERNEL} && \
			make KERNEL=${KERNEL} DESTDIR=${RD}/kernels \
				kernel-reinstall.debug

#
# --==## Put a filesystem into a BOOTMFS kernel ##==--
#
doMFSKERN:
	@echo "Running doMFSKERN for ${FSIMAGE}"
	@rm -f ${RD}/kernels/BOOTMFS.${FSIMAGE}
	@cd ${.CURDIR} && ${MAKE} createBOOTMFS 
	@cd ${.CURDIR} && ${MAKE} doKERNEL KERNEL=BOOTMFS KERNEL_KO=BOOTMFS KODIR=""
	@rm -rf ${RD}/image.${FSIMAGE}
	@mkdir  ${RD}/image.${FSIMAGE}
	@cd ${RD}/kernels && \
	  (chflags noschg BOOTMFS || true) && \
	  strip BOOTMFS && \
	  cp BOOTMFS BOOTMFS.${FSIMAGE} && \
	  [ -r BOOTMFS.hints ] && mv BOOTMFS.hints BOOTMFS.${FSIMAGE}.hints
	mv ${RD}/kernels/BOOTMFS ${RD}/image.${FSIMAGE}/kernel
	@echo "Setting up /boot directory for ${FSIMAGE} floppy"
	@mkdir -p ${RD}/image.${FSIMAGE}/boot
	@cp /boot/loader ${RD}/image.${FSIMAGE}/boot
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
	@echo "load -t mfs_root /mfsroot" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "echo \\007\\007" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "autoboot 10" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.if ${MACHINE_ARCH} == "i386" && ${AUTO_KEYBOARD_DETECT}
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
