# $FreeBSD: src/release/Makefile,v 1.536.2.23 2000/09/23 17:15:17 jkh Exp $
#
# make release CHROOTDIR=/some/dir BUILDNAME=somename [ RELEASETAG=tag ]
#
# Where "/some/dir" is the pathname of a directory on a some
# filesystem with at least 1000MB of free space, "somename" is what
# you want the release to call itself and, optionally, which CVS "tag"
# name should be used when checking out the sources to build the release
# (default is HEAD).
#
# Please note: the vn driver must also be compiled into your kernel,
# otherwise the target 'release.8' and possibly others will fail.
#
# Set these, release builder!
#
# Fixed version:
#BUILDNAME=4.0-RELEASE
#
# Automatic SNAP versioning:
DATE != date +%Y%m%d
BASE = 4.0
BUILDNAME?=${BASE}-${DATE}-STABLE
#
#CHROOTDIR=/junk/release
# If this is a -stable snapshot, then set
#RELEASETAG=RELENG_4
#
# Non-zero if ${RELEASETAG} is in the form "RELENG_ver_RELEASE"; we
# are building an official release.  Otherwise, we are building for
# a branch.
.if defined(RELEASETAG)
ISRELEASE!=	expr ${RELEASETAG} : '^RELENG_.*_RELEASE$$' || true
.if ${ISRELEASE} != 0
# Convert "RELENG_ver_RELEASE" to "RELEASE_ver" for ports and doc trees.
AUXRELEASETAG!=	echo ${RELEASETAG} | sed -e 's/^RELENG_/RELEASE_/' -e 's/_RELEASE$$//'
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

# Uncomment this to disable the doc.1 target.  It is also an ERROR
# to set NOPORTS and not set NODOC since docs depend on ports.
#NODOC=  YES
#NOPORTS=  YES
# Set ALLLANG=no if you want the release documentation to be
# in English only.
ALLLANG?=	yes
DOCPORTS=	textproc/docproj
# Set this to wherever the distfiles required by ${DOCPORTS} live.
DOCDISTFILES?=	${.CURDIR}/../../ports/distfiles
# Set this to 1 if you want -P to be used for automatic keyboard detection
# on the boot floppy.  WARNING: Breaks on some Athlon (K7) motherboards.
AUTO_KEYBOARD_DETECT?= 0


DIST_DOCS= ABOUT.TXT ERRATA.TXT LAYOUT.TXT README.TXT HARDWARE.TXT \
           RELNOTES.TXT TROUBLE.TXT UPGRADE.TXT INSTALL.TXT

# Things which without too much trouble can be considered variables
# BIN_DISTS are special in that they get full /etc installation sets.
#
COMPAT_DISTS?=	compat1x compat20 compat21 compat22 compat3x
OTHER_DISTS?=	manpages catpages games proflibs dict info doc
CRYPTO_DISTS?=	crypto krb4 krb5
BIN_DISTS?=	bin
DISTRIBUTIONS?=	${BIN_DISTS} ${OTHER_DISTS} ${COMPAT_DISTS} ${CRYPTO_DISTS}
KERNELS?=	GENERIC

# Extra source tarballs; each argument is a pair of source dir and
# distribution name.  The dist name should not exceed 7 characters
# (another "s" for "source" will be prepended).
EXTRA_SRC+=	usr.sbin/sendmail/cf smailcf

BOOT1=	etc/protocols etc/defaults/rc.conf

# mountpoint for filesystems.
MNT=			/mnt

# Various floppy image parameters.
#
.if ${MACHINE} == "pc98"
SMALLBOOTSIZE=		1200
BOOTSIZE=		1440
FIXITSIZE=		1440
MFSSIZE=		2880
BOOTINODE=		80000
FIXITINODE=		4000
MFSINODE=		8000
SMALLBOOTLABEL=		fd1200
BOOTLABEL=		fd1440
FIXITLABEL=		fd1440
MFSLABEL=		minimum2
.else
BOOTSIZE=		1440
FIXITSIZE=		1440
MFSSIZE=		2880
BIGBOOTSIZE=		2880
BOOTINODE=		80000
FIXITINODE=		4000
MFSINODE=		8000
BOOTLABEL=		fd1440
FIXITLABEL=		fd1440
MFSLABEL=		minimum2
BIGBOOTLABEL=		minimum2
.endif

# overrides.
.if ${MACHINE_ARCH} == "alpha"
FIXITSIZE=		2880
FIXITLABEL=		minimum2
.endif

ZIPNSPLIT=		gzip --no-name -9 -c | split -b 240640 -


VNDEVICE?=		vn0

# Things which may get you into trouble if you change them
MTREEFILES=		${.CURDIR}/../etc/mtree
_R=			/R
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

.if ${MACHINE} == "pc98"
EXTRAS= ftp.1
.else
EXTRAS= cdrom.1 ftp.1
.endif

.if !defined(NODOC)
DOCREL= doc.1
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
.if defined(NOPORTS) && !defined(NODOC)
	@echo "Ports are required for building the docs.  Either set NODOC or"
	@echo "unset NOPORTS!"
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
	@echo ">>> make release started on `LC_TIME=C TZ=GMT date`"
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
.if defined(AUXRELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co -P -r ${AUXRELEASETAG} ${RELEASEPORTSMODULE} && cd ports && ${MAKEREADMES}
.else
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -R -d ${CVSROOT} co -P ${RELEASEPORTSMODULE} && cd ports && ${MAKEREADMES}
.endif
.endif
.if !defined(NODOC)
.if defined(AUXRELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf doc && cvs -R -d ${CVSROOT} co -P -r ${AUXRELEASETAG} ${RELEASEDOCMODULE}
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
	echo "export DISTRIBUTIONS=\"${DISTRIBUTIONS}\"" >> ${CHROOTDIR}/mk
	echo "export BUILDNAME=${BUILDNAME}"	>> ${CHROOTDIR}/mk
	echo "export VNDEVICE=${VNDEVICE}"	>> ${CHROOTDIR}/mk
	echo "export OBJFORMAT=${OBJFORMAT}"	>> ${CHROOTDIR}/mk
	echo "export MODULES_WITH_WORLD=YES"	>> ${CHROOTDIR}/mk
.if defined(RELEASETAG)
	echo "export RELEASETAG=${RELEASETAG}"	>> ${CHROOTDIR}/mk
.endif
.if defined(NOPORTS)
	echo "export NOPORTS=${NOPORTS}"	>> ${CHROOTDIR}/mk
.endif
.if defined(NODOC)
	echo "export NODOC=${NODOC}"		>> ${CHROOTDIR}/mk
.endif
.if defined(ALLLANG) && ${ALLLANG} != "NO" && ${ALLLANG} != "no"
	echo "export ALLLANG=${ALLLANG}"	>> ${CHROOTDIR}/mk
.else
	echo "export DOC_LANG=en_US.ISO_8859-1"	>> ${CHROOTDIR}/mk
.endif
.if defined(NOSRC)
	echo "export NOSRC=${NOSRC}"		>> ${CHROOTDIR}/mk
.endif
.if defined(NOSHARED)
	echo "export NOSHARED=${NOSHARED}"	>> ${CHROOTDIR}/mk
.endif
.if defined(USA_RESIDENT)
	echo "export USA_RESIDENT=${USA_RESIDENT}" >> ${CHROOTDIR}/mk
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
	echo "cd /usr/src/release/sysinstall"	>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release"		>> ${CHROOTDIR}/mk
	echo "make \$${_RELTARGET}"		>> ${CHROOTDIR}/mk
	echo "echo \">>> make ${.TARGET} finished on \`LC_TIME=C TZ=GMT date\`\"" >> ${CHROOTDIR}/mk
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
	cd ${.CURDIR}/.. && make distribute DISTDIR=${RD}/trees
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
	rm -f ${RD}/kernels/${kernel}
	rm -rf ${.CURDIR}/../sys/compile/${kernel}
	cd ${.CURDIR} && ${MAKE} doKERNEL KERNEL=${kernel}
	rm -rf ${.CURDIR}/../sys/compile/${kernel}
	cp ${RD}/kernels/${kernel} ${RD}/trees/bin/kernel.${kernel}
.endfor
	touch release.3

# Make and install the three crunched binaries which live on the floppies.
# You are not supposed to like this :-)
#
# NB: the "RELEASE_BUILD_FIXIT" magic prevents vi from including the
# Tcl and Perl APIs.  See also /usr/src/usr.bin/vi/Makefile.
# It also prevents ls linking against termcap by disabling color support.
# See /usr/src/bin/ls/Makefile.
release.4:
	@mkdir -p /stand
	cd ${.CURDIR}/sysinstall && make obj depend all install
	rm -rf ${RD}/crunch
	mkdir -p ${RD}/crunch
	export RELEASE_BUILD_FIXIT=noway ; \
	for j in ${CRUNCH_TARGETS} ; do \
		rm -rf $${j}_crunch && \
		mkdir $${j}_crunch && \
		( cd $${j}_crunch && \
		  ( ( [ -f ${.CURDIR}/${MACHINE}/$${j}_crunch.conf ] && \
		    crunchgen ${.CURDIR}/${MACHINE}/$${j}_crunch.conf ) || \
		    ( crunchgen ${.CURDIR}/$${j}_crunch.conf ) ) && \
		  ${MAKE} -DRELEASE_CRUNCH -f $${j}_crunch.mk subclean all \
		    NOCRYPT=yes "CFLAGS=${CFLAGS} -DCRUNCHED_BINARY") && \
		mv $${j}_crunch/$${j}_crunch ${RD}/crunch/$${j} && \
		true || { rm -rf $${j}_crunch ; false ; } ; \
	done
	touch release.4

#
# --==## Fix up the distributions. ##==--
#
release.5:
	# Handle some grief caused by the munition braindeadness.
	for i in sbin/init bin/ed usr.sbin/ppp usr.sbin/pppd usr.sbin/tcpdump/tcpdump ; do \
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

	# Create symlinks for the MD5-based crypt lib, too.  The
	# automatically created links still point to the DES stuff,
	# which went into its own distribution.
	for i in ${RD}/trees/bin/usr/lib/libscrypt* ; do \
		c=`echo $$i | sed -e 's/libscrypt/libcrypt/'` ; \
		rm -f $$c ; \
		ln -s `basename $$i` $$c ; \
	done

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
		if [ -f krb4/krb4.aa ] ; then mv krb4/* crypto && rmdir krb4 ; fi )
	( cd ${RD}/dists && \
		if [ -f krb5/krb5.aa ] ; then mv krb5/* crypto && rmdir krb5 ; fi )
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
		if [ -f skrb5.aa ] ; then mv skrb5.* ../crypto ; fi ; ) ; fi
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
# +-----+-----+-+--------------------------------------------------------+-+
#               |kernel                                                  |
#               +------------+-----------------------------------------+-+
#                            |mfs filesystem "mfsfd"                   |
#                            +-----------------------------------------+
#

release.8: write_mfs_in_kernel
	rm -rf ${RD}/mfsfd
	mkdir ${RD}/mfsfd
	cd ${RD}/mfsfd && \
		mkdir -p etc/defaults dev mnt stand/help
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=boot \
		DIR=${RD}/mfsfd/stand ZIP=false
	( cd ${RD}/trees/bin/dev && \
		ls console tty bpf0 ttyv0 ttyv1 ttyv2 ttyv3 null zero card0 card1 card2 card3 usb usb0 uhid0 ums0 ulpt0 ugen0 kbd0 kmem mem | \
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
	echo "nameserver      42/tcp name"	> ${RD}/mfsfd/stand/etc/services
	echo "ftp             21/tcp"		>> ${RD}/mfsfd/stand/etc/services
	echo "domain          53/tcp          nameserver" >> ${RD}/mfsfd/stand/etc/services
	echo "domain          53/udp          nameserver" >> ${RD}/mfsfd/stand/etc/services
	echo "cmd             514/tcp    shell" >> ${RD}/mfsfd/stand/etc/services
	gzip -9c ${.CURDIR}/../COPYRIGHT > ${RD}/mfsfd/stand/help/COPYRIGHT.hlp.gz
	@for i in README.TXT RELNOTES.TXT INSTALL.TXT UPGRADE.TXT HARDWARE.TXT; do \
	  if [ -f ${.CURDIR}/texts/${MACHINE_ARCH}/$${i} ]; then \
	     gzip -9c ${.CURDIR}/texts/${MACHINE_ARCH}/$${i} > ${RD}/mfsfd/stand/help/$${i}.gz; \
	  else \
	     gzip -9c ${.CURDIR}/texts/$${i} > ${RD}/mfsfd/stand/help/$${i}.gz; \
	  fi; \
	done
	-test -f ${.CURDIR}/install.cfg && cp ${.CURDIR}/install.cfg ${RD}/mfsfd
	@mkdir -p ${RD}/mfsfd/boot
	@cp /boot/boot* ${RD}/mfsfd/boot
	@cp /boot/loader.help ${RD}/mfsfd/boot
	@echo "Making the regular boot floppy."
	@tar --exclude CVS -cf - -C ${.CURDIR}/sysinstall help | \
		tar xf - -C ${RD}/mfsfd/stand
	@echo "Compressing doc files..."
	@gzip -9 ${RD}/mfsfd/stand/help/*.hlp
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
	@rm mfsroot mfsroot.gz mfsroot.size
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
	@cp ${.CURDIR}/texts/FLOPPIES.TXT ${RD}/floppies/README.TXT
	@(cd ${RD}/floppies; md5 README.TXT *.flp > CHECKSUM.MD5)
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
	@for i in ${DIST_DOCS}; do \
	  if [ -f ${.CURDIR}/texts/$${i} ]; then \
	     cp ${.CURDIR}/texts/$${i} ${FD}; \
	  fi; \
	  if [ -f ${.CURDIR}/texts/${MACHINE_ARCH}/$${i} ]; then \
	     echo "=== Platform specifics for ${MACHINE_ARCH}" >> ${FD}/$${i}; \
	     cat ${.CURDIR}/texts/${MACHINE_ARCH}/$${i} >> ${FD}/$${i}; \
	  fi; \
	done
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
	@ln -f ${RD}/kernels/MFSKERNEL.boot ${CD_DISC1}/kernel
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
	@for i in ${DIST_DOCS}; do \
	  if [ -f ${.CURDIR}/texts/$${i} ]; then \
	     cp ${.CURDIR}/texts/$${i} ${CD_DISC1}; \
	  fi; \
	  if [ -f ${.CURDIR}/texts/${MACHINE_ARCH}/$${i} ]; then \
	     echo "=== Platform specifics for ${MACHINE_ARCH}" >> ${CD_DISC1}/$${i}; \
	     cat ${.CURDIR}/texts/${MACHINE_ARCH}/$${i} >> ${CD_DISC1}/$${i}; \
	  fi; \
        done
.if ${MACHINE_ARCH} == "alpha"
	@echo "Setting up Alpha CD disc1 for booting"
	@cp -Rp ${RD}/image.boot/boot ${CD_DISC1}
	@ln -f ${CD_DISC2}/boot/cdboot ${CD_DISC1}/boot
	@ln -f ${CD_DISC1}/boot/loader.rc ${CD_DISC2}/boot
	@ln -f ${CD_DISC1}/kernel ${CD_DISC2}/kernel
.endif
.if !defined(NOPORTS)
	@-rm -rf /usr/ports/distfiles/*
	@mkdir -p ${CD_DISC1}/ports && \
	  tar --exclude CVS -czf ${CD_DISC1}/ports/ports.tgz -C /usr ports && \
	  cp ${.CURDIR}/scripts/ports-install.sh ${CD_DISC1}/ports/install.sh \
	  && (cd ${CD_DISC1}/ports; md5 * > CHECKSUM.MD5)
.endif
	touch cdrom.1

doc.1:
	@echo "Making docs..."
	@for i in ${DOCPORTS}; do \
	  cd /usr/ports/$$i && make all install clean JADETEX=no FORCE_PKG_REGISTER=yes; \
	done
	@cd /usr/doc && make all install 'FORMATS=html html-split txt' INSTALL_COMPRESSED='' DOCDIR=${RD}/trees/bin/usr/share/doc
	touch doc.1

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
	@echo "TB undefined in doTARBALL" && exit 1
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
# --==## Compile a kernel by name ${KERNEL} ##==--
#
# We don't erase the sys/compile/${KERNEL} directory, since somebody
# may want to reuse it (release.8 presently)
#
doKERNEL:
	@rm -f ${RD}/kernels/${KERNEL}
	@cd ${.CURDIR}/../sys/${MACHINE}/conf && config ${KERNEL}
	@cd ${.CURDIR}/../sys/compile/${KERNEL} && \
			make kernel-depend && \
			make ${KERNEL_FLAGS} ${KERNEL} && \
			make kernel-reinstall DESTDIR=${RD}/kernels

#
# --==## Put a filesystem into a BOOTMFS kernel ##==--
#
doMFSKERN:
	@echo "Running doMFSKERN for ${FSIMAGE}"
	@rm -f ${RD}/kernels/BOOTMFS.${FSIMAGE}
	@cd ${.CURDIR}/../sys/${MACHINE}/conf && \
	  sh ${.CURDIR}/scripts/dokern.sh ${FDSIZE} < ${KERNCONF} > BOOTMFS
.if ${MACHINE_ARCH} == "i386"
	@echo "options  INTRO_USERCONFIG" >> \
	  ${.CURDIR}/../sys/${MACHINE}/conf/BOOTMFS
.endif
.if defined(FDSIZE) && ${FDSIZE} == "BIG"
	@echo "options MD_ROOT_SIZE=`cat mfsroot.size`" >> \
	  ${.CURDIR}/../sys/${MACHINE}/conf/BOOTMFS
.endif
	@cd ${.CURDIR} && ${MAKE} doKERNEL KERNEL=BOOTMFS
	@rm -rf ${RD}/image.${FSIMAGE}
	@mkdir  ${RD}/image.${FSIMAGE}
	@chflags noschg ${RD}/kernels/BOOTMFS
	@strip ${RD}/kernels/BOOTMFS
	@cp ${RD}/kernels/BOOTMFS ${RD}/kernels/BOOTMFS.${FSIMAGE}
	mv ${RD}/kernels/BOOTMFS ${RD}/image.${FSIMAGE}/kernel
	@echo "Setting up /boot directory for ${FSIMAGE} floppy"
	@mkdir -p ${RD}/image.${FSIMAGE}/boot
	@cp /boot/boot[12] /boot/loader ${RD}/image.${FSIMAGE}/boot
.if !defined(FDSIZE) || ${FDSIZE} != "BIG"
	@echo "load /kernel" > ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "echo \\007\\007" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "echo Please insert MFS root floppy and press enter:" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "read" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "load -t mfs_root /mfsroot" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.endif
	@echo "echo \\007\\007" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
	@echo "autoboot 10" >> ${RD}/image.${FSIMAGE}/boot/loader.rc
.if ${MACHINE_ARCH} == "i386" && ${AUTO_KEYBOARD_DETECT}
	@echo "-P" >> ${RD}/image.${FSIMAGE}/boot.config
.endif
.if defined(FDSIZE) && ${FDSIZE} == "BIG"
	@echo "Writing MFS image into kernel for ${FSIMAGE} floppy"
	@./write_mfs_in_kernel ${RD}/image.${FSIMAGE}/kernel mfsroot
	@cp ${RD}/image.${FSIMAGE}/kernel ${RD}/kernels/MFSKERNEL.${FSIMAGE}
.endif
	@gzip -9v ${RD}/image.${FSIMAGE}/kernel
	@rm -f ${RD}/floppies/${FSIMAGE}.flp
.if defined(FDSIZE) && ${FDSIZE} == "BIG"
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
