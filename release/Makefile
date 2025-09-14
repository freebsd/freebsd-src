#
# Makefile for building releases and release media.
#
# User-driven targets:
#  cdrom: Builds release CD-ROM media (disc1.iso)
#  dvdrom: Builds release DVD-ROM media (dvd1.iso)
#  memstick: Builds memory stick image (memstick.img)
#  mini-memstick: Builds minimal memory stick image (mini-memstick.img)
#  ftp: Sets up FTP distribution area (ftp)
#  release: Invokes real-release, vm-release, cloudware-release and oci-release targets
#  real-release: Build all media and FTP distribution area
#  vm-release: Build all virtual machine image targets
#  cloudware-release: Build all cloud hosting provider targets
#  oci-release: Build all OCI container images
#  install: Invokes the release-install, vm-install and oci-install targets
#  release-install: Copies all release installation media into ${DESTDIR}
#  vm-install: Copies all virtual machine images into ${DESTDIR}
#  cloud-install: Copies non-uploaded cloud images into ${DESTDIR}
#  oci-install: Copies all OCI container images into ${DESTDIR}
#
# Variables affecting the build process:
#  WORLDDIR: location of src tree -- must have built world and default kernel
#            (by default, the directory above this one)
#  NODISTSETS: if set, do not include dist sets or MANIFEST
#  PKGBASE:  if set, include pkgbase packages rather than dist tarballs in
#            disc1 and dvd1 installation media
#  PORTSDIR: location of ports tree to distribute (default: /usr/ports)
#  XTRADIR:  xtra-bits-dir argument for <arch>/mkisoimages.sh
#  NOPKG:    if set, do not distribute third-party packages
#  NOPORTS:  if set, do not distribute ports tree
#  NOSRC:    if set, do not distribute source tree
#  WITH_DVD: if set, generate dvd1.iso
#  WITH_COMPRESSED_IMAGES: if set, compress installation images with xz(1)
#		(uncompressed images are not removed)
#  WITH_VMIMAGES: if set, build virtual machine images with the release
#  WITH_COMPRESSED_VMIMAGES: if set, compress virtual machine disk images
#  		with xz(1) (extremely time consuming)
#  WITH_CLOUDWARE: if set, build cloud hosting disk images with the release
#  TARGET/TARGET_ARCH: architecture of built release
#

WORLDDIR?=	${.CURDIR}/..
PORTSDIR?=	/usr/ports

.include "${WORLDDIR}/share/mk/bsd.compat.pre.mk"

.if !defined(TARGET) || empty(TARGET)
TARGET=		${MACHINE}
.endif
.if !defined(TARGET_ARCH) || empty(TARGET_ARCH)
.if ${TARGET} == ${MACHINE}
TARGET_ARCH=	${MACHINE_ARCH}
.else
TARGET_ARCH=	${TARGET}
.endif
.endif
IMAKE=		${MAKE} TARGET_ARCH=${TARGET_ARCH} TARGET=${TARGET} \
		    -DNO_ROOT -DDB_FROM_SRC
DISTDIR=	dist

# Define OSRELEASE by using newvers.sh
.if !defined(OSRELEASE) || empty(OSRELEASE)
.for _V in TYPE BRANCH REVISION
${_V}!=	eval $$(awk '/^${_V}=/{print}' ${.CURDIR}/../sys/conf/newvers.sh); echo $$${_V}
.endfor
.for _V in ${TARGET_ARCH}
.if !empty(TARGET:M${_V})
OSRELEASE=	${TYPE}-${REVISION}-${BRANCH}-${TARGET}
VOLUME_LABEL=	${REVISION:C/[.-]/_/g}_${BRANCH:C/[.-]/_/g}_${TARGET}
.else
OSRELEASE=	${TYPE}-${REVISION}-${BRANCH}-${TARGET}-${TARGET_ARCH}
VOLUME_LABEL=	${REVISION:C/[.-]/_/g}_${BRANCH:C/[.-]/_/g}_${TARGET_ARCH}
.endif
.endfor
.endif

.if ${.MAKE.OS} == "FreeBSD"
# For installing packages into the release media.
PKG_ENV+=	INSTALL_AS_USER=yes
PKG_ENV+=	ASSUME_ALWAYS_YES=yes
PKG_ARGS+=	-o METALOG=METALOG
PKG_ARGS+=	-r ${.TARGET} -o REPOS_DIR=${.CURDIR}/pkg_repos
# Pass -f to make sure pkg writes to the METALOG even if the package
# is already installed from a previous build
PKG_INSTALL=	env ${PKG_ENV} ${PKG_CMD} ${PKG_ARGS} install -f
PKG_CLEAN=	env ${PKG_ENV} ${PKG_CMD} ${PKG_ARGS} clean -a
.endif

.if !defined(VOLUME_LABEL) || empty(VOLUME_LABEL)
VOLUME_LABEL=	FreeBSD_Install
.endif

.if !exists(${PORTSDIR})
NOPORTS= true
.endif

DISTRIBUTIONS= base.txz kernel.txz
.if !defined(NOPORTS)
DISTRIBUTIONS+= ports.txz
.endif
.if !defined(NOSRC)
DISTRIBUTIONS+= src.txz
.endif

RELEASE_TARGETS= ftp
IMAGES=
.if exists(${.CURDIR}/${TARGET}/mkisoimages.sh)
RELEASE_TARGETS+= cdrom
IMAGES+=	disc1.iso bootonly.iso
. if defined(WITH_DVD) && !empty(WITH_DVD)
RELEASE_TARGETS+= dvdrom
IMAGES+=	dvd1.iso
. endif
.endif
.if exists(${.CURDIR}/${TARGET}/make-memstick.sh)
RELEASE_TARGETS+= memstick.img
RELEASE_TARGETS+= mini-memstick.img
IMAGES+=	memstick.img
IMAGES+=	mini-memstick.img
.endif

CLEANFILES=	packagesystem *.txz MANIFEST release ${IMAGES}
.if defined(WITH_COMPRESSED_IMAGES) && !empty(WITH_COMPRESSED_IMAGES)
. for I in ${IMAGES}
CLEANFILES+=	${I}.xz
. endfor
.endif
.if defined(WITH_DVD) && !empty(WITH_DVD)
CLEANFILES+=	pkg-stage
.endif
CLEANDIRS=	dist pkgdb pkgbase-repo pkgbase-repo-conf ftp disc1 disc1-disc1 disc1-memstick bootonly bootonly-bootonly bootonly-memstick dvd
beforeclean:
	chflags -R noschg .
.include <bsd.obj.mk>
clean: beforeclean

#
# Legacy distribution tarballs
#
base.txz: # Also (if enabled) tests, compat libs, and -dbg variants for all.
	mkdir -p ${DISTDIR}
	( cd ${WORLDDIR} && ${IMAKE} distributeworld DISTDIR=${.OBJDIR}/${DISTDIR} )
	# Bootstrap etcupdate(8) database.
	sh ${WORLDDIR}/usr.sbin/etcupdate/etcupdate.sh extract -B \
	    -m "${MAKE}" -M "TARGET_ARCH=${TARGET_ARCH} TARGET=${TARGET}" \
	    -s ${WORLDDIR} -d "${.OBJDIR}/${DISTDIR}/base/var/db/etcupdate" \
	    -L /dev/stdout -N
	echo "./var/db/etcupdate type=dir uname=root gname=wheel mode=0755" >> ${.OBJDIR}/${DISTDIR}/base.meta
	sed -n 's,^\.,./var/db/etcupdate/current,p' \
	    ${.OBJDIR}/${DISTDIR}/base/var/db/etcupdate/current/METALOG | \
	    env -i LC_COLLATE=C sort >> ${.OBJDIR}/${DISTDIR}/base.meta
	rm ${.OBJDIR}/${DISTDIR}/base/var/db/etcupdate/current/METALOG
# Package all components
	( cd ${WORLDDIR} && ${IMAKE} packageworld DISTDIR=${.OBJDIR}/${DISTDIR} )
	mv ${DISTDIR}/*.txz .

kernel.txz: # Also (if enabled) kernel-dbg.txz.
	mkdir -p ${DISTDIR}
	( cd ${WORLDDIR} && ${IMAKE} distributekernel DISTDIR=${.OBJDIR}/${DISTDIR} )
	( cd ${WORLDDIR} && ${IMAKE} packagekernel DISTDIR=${.OBJDIR}/${DISTDIR} )
	mv ${DISTDIR}/kernel*.txz .

src.txz:
	mkdir -p ${DISTDIR}/usr
	rm -f ${DISTDIR}/usr/src
	ln -s ${WORLDDIR} ${DISTDIR}/usr/src
	( cd ${DISTDIR} && ${TAR_XZ_CMD} -cLvf ${.OBJDIR}/src.txz \
	    --exclude .svn --exclude .zfs \
	    --exclude .git --exclude @ --exclude usr/src/release/dist \
	    --exclude usr/src/release/obj usr/src )

ports.txz:
	mkdir -p ${DISTDIR}/usr
	rm -f ${DISTDIR}/usr/ports
	ln -s ${PORTSDIR} ${DISTDIR}/usr/ports
	( cd ${DISTDIR} && ${TAR_XZ_CMD} -cLvf ${.OBJDIR}/ports.txz \
	    --exclude .git --exclude .svn \
	    --exclude usr/ports/distfiles --exclude usr/ports/packages \
	    --exclude 'usr/ports/INDEX*' --exclude work usr/ports )

packagesystem: ${DISTRIBUTIONS}
	sh ${.CURDIR}/scripts/make-manifest.sh $$(ls *.txz | grep -v container) > MANIFEST
	touch ${.TARGET}

.if !defined(NODISTSETS)
disc1: packagesystem
bootonly: packagesystem
dvd: packagesystem
.endif

.if defined(PKGBASE)
PKGBASE_REPO=	pkgbase-repo
PKG_ABI=	$$(${PKG_CMD} -o ABI_FILE=${.TARGET}/usr/bin/uname config ABI)
.endif

pkgbase-repo:
	mkdir -p pkgbase-repo
	( ${IMAKE} -C ${WORLDDIR} packages REPODIR=${.OBJDIR}/pkgbase-repo )

disc1: ${PKGBASE_REPO}
# Install system
	mkdir -p ${.TARGET}
	( cd ${WORLDDIR} && ${IMAKE} installworld installkernel distribution \
	    DESTDIR=${.OBJDIR}/${.TARGET} MK_AT=no \
	    MK_INSTALLLIB=no MK_MAIL=no \
	    ${_ALL_LIBCOMPATS:@v@MK_LIB$v=no@} \
	    MK_TOOLCHAIN=no \
	    MK_RESCUE=no MK_DICT=no \
	    MK_KERNEL_SYMBOLS=no MK_TESTS=no MK_DEBUG_FILES=no)
.if defined(PKGBASE)
# Create offline pkgbase repo on release media
	mkdir -p ${.TARGET}/usr/freebsd-packages/repos/
	${.CURDIR}/scripts/pkgbase-stage.lua disc \
	    ${.OBJDIR}/pkgbase-repo/${PKG_ABI}/latest \
	    ${.TARGET}/usr/freebsd-packages/offline \
	    "${_ALL_libcompats}" ${PKG_ABI}
	cp ${.CURDIR}/scripts/FreeBSD-base-offline.conf \
		${.TARGET}/usr/freebsd-packages/repos/
	mtree -c -p ${.TARGET}/usr/freebsd-packages | \
	    mtree -C -k type,mode,link,size | \
	    sed 's|^\.|./usr/freebsd-packages|g' >> ${.TARGET}/METALOG
.endif
.if !defined(NODISTSETS)
	# Copy MANIFEST to provide legacy dist checksums in both modes
	mkdir -p ${.TARGET}/usr/freebsd-dist
	echo "./usr/freebsd-dist type=dir uname=root gname=wheel mode=0755" >> ${.TARGET}/METALOG
	cp MANIFEST ${.TARGET}/usr/freebsd-dist
	echo "./usr/freebsd-dist/MANIFEST type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
.if !defined(PKGBASE)
# Copy distfiles
	for dist in $$(ls *.txz | grep -v container | grep -vE -- '(${base ${_ALL_libcompats}:L:ts|})-dbg'); do \
	    cp $${dist} ${.TARGET}/usr/freebsd-dist; \
	    echo "./usr/freebsd-dist/$${dist} type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG; \
	done
.endif
.endif
.if ${.MAKE.OS} == "FreeBSD" && (!defined(NOPKG) || empty(NOPKG))
# Install packages onto release media.
	${PKG_INSTALL} pkg || true
	${PKG_INSTALL} wifi-firmware-kmod-release || true
	${PKG_CLEAN} || true
.endif
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${.TARGET}/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${.TARGET}/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${.TARGET}/etc/rc.conf
	echo vfs.mountroot.timeout=\"10\" >> ${.TARGET}/boot/loader.conf
	echo kernels_autodetect=\"NO\" >> ${.TARGET}/boot/loader.conf
	echo loader_brand=\"install\" >> ${.TARGET}/boot/loader.conf
	echo loader_menu_multi_user_prompt=\"Installer\" >> ${.TARGET}/boot/loader.conf
	cp ${.CURDIR}/rc.local ${.TARGET}/etc
	echo "./etc/resolv.conf type=link uname=root gname=wheel mode=0644 link=/tmp/bsdinstall_etc/resolv.conf" >> ${.TARGET}/METALOG
	echo "./etc/rc.conf type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	echo "./etc/sysctl.conf type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	echo "./boot/loader.conf type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	echo "./etc/rc.local type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	touch ${.TARGET}

bootonly:
# Install system
	mkdir -p ${.TARGET}
	( cd ${WORLDDIR} && ${IMAKE} installworld installkernel distribution \
	    DESTDIR=${.OBJDIR}/${.TARGET} MK_AT=no \
	    MK_GAMES=no \
	    MK_INSTALLLIB=no MK_MAIL=no \
	    ${_ALL_LIBCOMPATS:@v@MK_LIB$v=no@} \
	    MK_TOOLCHAIN=no \
	    MK_RESCUE=no MK_DICT=no \
	    MK_KERNEL_SYMBOLS=no MK_TESTS=no MK_DEBUG_FILES=no)
.if !defined(NODISTSETS)
# Copy manifest only (no distfiles) to get checksums
	mkdir -p ${.TARGET}/usr/freebsd-dist
	cp MANIFEST ${.TARGET}/usr/freebsd-dist
	echo "./usr/freebsd-dist type=dir uname=root gname=wheel mode=0755" >> ${.TARGET}/METALOG
	echo "./usr/freebsd-dist/MANIFEST type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
.endif
.if ${.MAKE.OS} == "FreeBSD" && (!defined(NOPKG) || empty(NOPKG))
# Install packages onto release media.
	${PKG_INSTALL} pkg || true
	${PKG_INSTALL} wifi-firmware-iwlwifi-kmod wifi-firmware-rtw88-kmod || true
	${PKG_CLEAN} || true
.endif
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${.TARGET}/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${.TARGET}/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${.TARGET}/etc/rc.conf
	echo vfs.mountroot.timeout=\"10\" >> ${.TARGET}/boot/loader.conf
	echo kernels_autodetect=\"NO\" >> ${.TARGET}/boot/loader.conf
	echo loader_brand=\"install\" >> ${.TARGET}/boot/loader.conf
	echo loader_menu_multi_user_prompt=\"Installer\" >> ${.TARGET}/boot/loader.conf
	cp ${.CURDIR}/rc.local ${.TARGET}/etc
	echo "./etc/resolv.conf type=link uname=root gname=wheel mode=0644 link=/tmp/bsdinstall_etc/resolv.conf" >> ${.TARGET}/METALOG
	echo "./etc/rc.conf type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	echo "./etc/sysctl.conf type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	echo "./boot/loader.conf type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	echo "./etc/rc.local type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG

dvd: ${PKGBASE_REPO}
# Install system
	mkdir -p ${.TARGET}
	( cd ${WORLDDIR} && ${IMAKE} installworld installkernel distribution \
		DESTDIR=${.OBJDIR}/${.TARGET} MK_RESCUE=no MK_KERNEL_SYMBOLS=no \
		MK_TESTS=no MK_DEBUG_FILES=no)
.if defined(PKGBASE)
# Create offline pkgbase repo on release media
	mkdir -p ${.TARGET}/usr/freebsd-packages/repos/
	${.CURDIR}/scripts/pkgbase-stage.lua dvd \
	    ${.OBJDIR}/pkgbase-repo/${PKG_ABI}/latest \
	    ${.TARGET}/usr/freebsd-packages/offline \
	    "${_ALL_libcompats}"
	cp ${.CURDIR}/scripts/FreeBSD-base-offline.conf \
		${.TARGET}/usr/freebsd-packages/repos/
	mtree -c -p ${.TARGET}/usr/freebsd-packages | \
	    mtree -C -k type,mode,link,size | \
	    sed 's|^\.|./usr/freebsd-packages|g' >> ${.TARGET}/METALOG
.endif
.if !defined(NODISTSETS)
	# Copy MANIFEST to provide legacy dist checksums in both modes
	mkdir -p ${.TARGET}/usr/freebsd-dist
	echo "./usr/freebsd-dist type=dir uname=root gname=wheel mode=0755" >> ${.TARGET}/METALOG
	cp MANIFEST ${.TARGET}/usr/freebsd-dist
	echo "./usr/freebsd-dist/MANIFEST type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
.if !defined(PKGBASE)
# Copy distfiles
	mkdir -p ${.TARGET}/usr/freebsd-dist
	for dist in $$(ls *.txz | grep -v container); do \
	    cp $${dist} ${.TARGET}/usr/freebsd-dist; \
	    echo "./usr/freebsd-dist/$${dist} type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG; \
	done
.endif
.endif
.if ${.MAKE.OS} == "FreeBSD" && (!defined(NOPKG) || empty(NOPKG))
# Install packages onto release media.
	${PKG_INSTALL} pkg || true
	${PKG_INSTALL} wifi-firmware-kmod-release || true
	${PKG_CLEAN} || true
.endif
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${.TARGET}/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${.TARGET}/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${.TARGET}/etc/rc.conf
	echo vfs.mountroot.timeout=\"10\" >> ${.TARGET}/boot/loader.conf
	echo kernels_autodetect=\"NO\" >> ${.TARGET}/boot/loader.conf
	echo loader_brand=\"install\" >> ${.TARGET}/boot/loader.conf
	echo loader_menu_multi_user_prompt=\"Installer\" >> ${.TARGET}/boot/loader.conf
	cp ${.CURDIR}/rc.local ${.TARGET}/etc
	echo "./etc/resolv.conf type=link uname=root gname=wheel mode=0644 link=/tmp/bsdinstall_etc/resolv.conf" >> ${.TARGET}/METALOG
	echo "./etc/rc.conf type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	echo "./etc/sysctl.conf type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	echo "./boot/loader.conf type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	echo "./etc/rc.local type=file uname=root gname=wheel mode=0644" >> ${.TARGET}/METALOG
	touch ${.TARGET}

disc1-disc1 disc1-memstick: disc1
	mkdir ${.TARGET}
	tar -cf- -C disc1 . | tar -xf- -C ${.TARGET}

bootonly-bootonly bootonly-memstick: bootonly
	mkdir ${.TARGET}
	tar -cf- -C bootonly . | tar -xf- -C ${.TARGET}

release.iso: disc1.iso
disc1.iso: disc1-disc1
	cd disc1-disc1 && sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b ${VOLUME_LABEL}_CD ../${.TARGET} ./METALOG ${XTRADIR}

dvd1.iso: dvd pkg-stage
	cd dvd && sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b ${VOLUME_LABEL}_DVD ../${.TARGET} ./METALOG ${XTRADIR}

bootonly.iso: bootonly-bootonly
	cd bootonly-bootonly && sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b ${VOLUME_LABEL}_BO ../${.TARGET} ./METALOG ${XTRADIR}

memstick: memstick.img
memstick.img: disc1-memstick
	cd disc1-memstick && sh ${.CURDIR}/${TARGET}/make-memstick.sh ./METALOG ../${.TARGET}

mini-memstick: mini-memstick.img
mini-memstick.img: bootonly-memstick
	cd bootonly-memstick && sh ${.CURDIR}/${TARGET}/make-memstick.sh ./METALOG ../${.TARGET}

pkg-stage: dvd
.if !defined(NOPKG) || empty(NOPKG)
	env PORTSDIR=${PORTSDIR} REPOS_DIR=${.CURDIR}/pkg_repos/ \
		sh ${.CURDIR}/scripts/pkg-stage.sh -N
	mkdir -p ${.OBJDIR}/dvd/packages/repos/
	cp ${.CURDIR}/scripts/FreeBSD_install_cdrom.conf \
		${.OBJDIR}/dvd/packages/repos/
.endif
	touch ${.TARGET}

cdrom: disc1.iso bootonly.iso
dvdrom: dvd1.iso
ftp: packagesystem
	rm -rf ftp
	mkdir -p ftp
	cp *.txz MANIFEST ftp
	rm -f ftp/container-*.txz

release:	real-release vm-release cloudware-release oci-release
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} release-done
	true

release-done:
	touch release

real-release:
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} obj
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} ${RELEASE_TARGETS}

install:	release-install vm-install oci-install .WAIT cloud-install

release-install:
.if defined(DESTDIR) && !empty(DESTDIR)
	mkdir -p ${DESTDIR}
.endif
	cp -a ftp ${DESTDIR}/
.if !empty(IMAGES)
.for I in ${IMAGES}
	cp -p ${I} ${DESTDIR}/${OSRELEASE}-${I}
. if defined(WITH_COMPRESSED_IMAGES) && !empty(WITH_COMPRESSED_IMAGES)
	${XZ_CMD} -k ${DESTDIR}/${OSRELEASE}-${I}
. endif
.endfor
	( cd ${DESTDIR} && sha512 ${OSRELEASE}* > ${DESTDIR}/CHECKSUM.SHA512 )
	( cd ${DESTDIR} && sha256 ${OSRELEASE}* > ${DESTDIR}/CHECKSUM.SHA256 )
.endif

.include "${.CURDIR}/Makefile.inc1"
.include "${.CURDIR}/Makefile.vm"
.include "${.CURDIR}/Makefile.oci"
