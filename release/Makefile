#	$Id: Makefile,v 1.290 1997/04/13 08:46:58 joerg Exp $
#
# How to roll a release:
#
#	make release CHROOTDIR=<lots of disk> [ RELEASETAG=something ]
#
# As far as I know, this will roll everything nicely into the "stage"
# directory.  I still need to write the two rules to move that into
# the "cdrom" and "ftp" directories.
# I also need to "make ports" and install those, but I'm not going to do
# that on a 14.4 line just yet...
#
# please note: the vn driver has to be compiled into your kernel,
# otherwise the target 'release.8' and possibly others will fail.
#
# SET THIS !!!
#BUILDNAME=3.0-970302-SNAP
#CHROOTDIR=/junk/release
# If this is a RELEASE, then set
#RELEASETAG=RELENG_2_2

# Things which without too much trouble can be considered variables

COMPAT_DISTS=	compat1x compat20 compat21
EXPORT_DISTS=	games manpages catpages proflibs dict info doc ${COMPAT_DISTS}
EXTRA_DISTS=	krb des ${EXPORT_DISTS}
ALL_DISTS=	bin ${EXTRA_DISTS}
KERNELS?=	GENERIC

# Extra source tarballs; each argument is a pair of source dir and
# distribution name.  The dist name should not exceed 7 characters
# (another "s" for "source" will be prepended).
EXTRA_SRC+=	usr.sbin/sendmail/cf smailcf

BOOT1=	etc/protocols etc/sysconfig

# mountpoint for filesystems.
MNT=			/mnt

# other floppy parameters.
FDSIZE=			1440
FDLABEL=		fd1440

ZIPNSPLIT=		gzip --no-name -9 -c | split -b 240640 -

# Upper size for the mfs in the boot.flp kernel.
# These are adjusted down to the minimum needed by doFS.sh.
BOOTMFSSIZE=		1440
MFSINODE=		7000
FIXITINODE=		2000
BOOTINODE=		100000

# Things which will get you into trouble if you change them
DISTRIBUTIONS=		bin ${EXTRA_DISTS}
MTREEFILES=		${.CURDIR}/../etc/mtree
RD=			/R/stage
FD=			/R/ftp
CD=			/R/cdrom
CD_DISC1=		${CD}/disc1
CD_DISC2=		${CD}/disc2

# ${BOOTSTRAPDIR} is for those utilities that refer to the hosting
# environment, rather then the target environment.  This is specifically
# intended for kernel-dependant utilities that are used during the build.
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

.if !defined(WHICH_CRUNCH)
WHICH_CRUNCH=	boot fixit
.endif

REDO?=		sysinstall
REDOSED=	sed -e 's/dirs/release.1/' -e 's/trees/release.2/' \
		    -e 's/kerns/release.3/' -e 's/sysinstall/release.4/' \
		    -e 's/fixup/release.5/' -e 's/tarbin/release.6/' \
		    -e 's/tarsrc/release.7/' -e 's/boot\.flp/release.8/' \
		    -e 's/fixit\.flp/release.9/' -e 's/ftp/ftp.1/' \
		    -e 's/cdrom/cdrom.1/'

REDOREDO!=	echo ${REDO} | ${REDOSED}

rerelease release:
.if !defined(CHROOTDIR) || !defined(BUILDNAME)
	@echo "To make a release you must set CHROOTDIR and BUILDNAME" && false
.endif
.if make(release)
.if exists(${CHROOTDIR})
	chflags -R noschg ${CHROOTDIR}/.
	-rm -rf ${CHROOTDIR}
.endif
	-mkdir -p ${CHROOTDIR}
	cd ${.CURDIR}/../etc && ${MAKE} distrib-dirs DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/../etc && ${MAKE} distribution DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/.. && ${MAKE} install DESTDIR=${CHROOTDIR} NOMAN=1
	mkdir ${CHROOTDIR}/${BOOTSTRAPDIR}
	for i in ${BOOTSTRAPUTILS} ; do \
		cp -p ${CHROOTDIR}$$i ${CHROOTDIR}/${BOOTSTRAPDIR} ; \
	done
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr && rm -rf src && \
		cvs -d ${CVSROOT} co -P src
.if defined(INTCVSROOT)
	cd ${CHROOTDIR}/usr && \
	    cvs -d ${INTCVSROOT} co -P src/secure src/eBones
.endif
.else
	cd ${CHROOTDIR}/usr && rm -rf src && \
		cvs -d ${CVSROOT} co -P -r ${RELEASETAG} src
.if defined(INTCVSROOT)
	cd ${CHROOTDIR}/usr && \
	    cvs -d ${INTCVSROOT} co -P -r ${RELEASETAG} src/secure src/eBones
.endif
.endif
.if defined(LOCAL_PATCHES) && exists(${LOCAL_PATCHES})
	cd ${CHROOTDIR}/usr/src && patch --silent < ${LOCAL_PATCHES}
.endif
.if !defined(NOPORTS)
	cd ${CHROOTDIR}/usr && rm -rf ports && cvs -d ${CVSROOT} co -P ports
.endif
.endif
.if make(rerelease)
.if !defined(RELEASENOUPDATE)
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr/src && cvs -q update -P -d
.else
	cd ${CHROOTDIR}/usr/src && cvs -q update -P -d -r ${RELEASETAG}
.endif
.if !defined(NOPORTS)
	cd ${CHROOTDIR}/usr/ports && cvs -q update -P -d
.endif
.endif
.endif
	( cd ${CHROOTDIR}/usr/src/sys/conf && \
	  mv newvers.sh foo && \
	  sed "s/^RELEASE=.*/RELEASE=${BUILDNAME}/" foo > newvers.sh&& rm foo )
	( cd ${CHROOTDIR}/usr/src/release/sysinstall && \
	  sed "s/__RELEASE/${BUILDNAME}/" version.h > version.h.new && \
	  mv version.h.new version.h && \
	  echo XXXX )
	-test -f install.cfg && cp install.cfg ${CHROOTDIR}/usr/src/release
	echo "#!/bin/sh"			> ${CHROOTDIR}/mk
	echo "set -ex"				>> ${CHROOTDIR}/mk
	echo "export CFLAGS='-O2 -pipe'"	>> ${CHROOTDIR}/mk
	echo "export BUILDNAME=${BUILDNAME}"	>> ${CHROOTDIR}/mk
.if defined(RELEASETAG)
	echo "export RELEASETAG=${RELEASETAG}"	>> ${CHROOTDIR}/mk
.endif
.if defined(NOPORTS)
	echo "export NOPORTS=${NOPORTS}"	>> ${CHROOTDIR}/mk
.endif
	# Don't remove this, or the build will fall over!
	echo "export RELEASEDIR=/R"		>> ${CHROOTDIR}/mk
	echo "export PATH=${BOOTSTRAPDIR}:$${PATH}" >> ${CHROOTDIR}/mk
	echo "cd /usr/src"			>> ${CHROOTDIR}/mk
.if make(release)
	# This eases bootstrapping from a more recent hosting environment:
	echo "mkdir -p /usr/lib/compat"		>> ${CHROOTDIR}/mk
	echo "chflags noschg /usr/lib/lib*.so.*" >> ${CHROOTDIR}/mk
	echo "mv /usr/lib/lib*.so.* /usr/lib/compat" >> ${CHROOTDIR}/mk
	echo "ldconfig /usr/lib /usr/lib/compat" >> ${CHROOTDIR}/mk
	echo "make world"			>> ${CHROOTDIR}/mk
	echo "(cd etc; make distribution)"	>> ${CHROOTDIR}/mk
	# Now we've got our own shared libs, remove the bootstrapping
	# libs again.
	echo "rm -f /usr/lib/compat/*"		>> ${CHROOTDIR}/mk
	echo "ldconfig /usr/lib"		>> ${CHROOTDIR}/mk
.endif
.if make(rerelease)
	echo "make all install"			>> ${CHROOTDIR}/mk
.endif
	echo "cd /usr/src/release/sysinstall"	>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release"		>> ${CHROOTDIR}/mk
	echo "make objlink"			>> ${CHROOTDIR}/mk
	echo "(cd obj; rm -f ${REDOREDO})"	>> ${CHROOTDIR}/mk
	echo "make doRELEASE"			>> ${CHROOTDIR}/mk
	echo "echo make ${.TARGET} Finished"	>> ${CHROOTDIR}/mk
	chmod 755 ${CHROOTDIR}/mk
	chroot ${CHROOTDIR} /mk

whap:
	( cd ${CHROOTDIR}/usr/src/release/sysinstall && \
	  sed "s/__RELEASE/${BUILDNAME}/" version.h > version.h.new && mv version.h.new version.h )

clean:
	rm -rf boot_crunch release.[0-9]

# Clean out /R and make the directory structure.
release.1:
	-mkdir /R
	chflags -R noschg /R/.
	rm -rf /R/*
	mkdir ${RD}
	mkdir ${RD}/floppies
	mkdir ${RD}/trees
	mkdir ${RD}/dists
	mkdir ${RD}/kernels
	for i in ${ALL_DISTS} ; do \
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
.if exists(${.CURDIR}/../eBones)
	cd ${.CURDIR}/../eBones && ( \
		make bootstrap &&\
		make obj all help-distribute DISTDIR=${RD}/trees &&\
		make kprog \
	)
.endif
	chflags -R noschg ${RD}/trees
	touch release.2

# Make and install the generic kernel(s).
release.3:
.for kernel in ${KERNELS}
	rm -f ${RD}/kernels/${kernel}
	rm -rf ${.CURDIR}/../sys/compile/${kernel}
	cd ${.CURDIR} && ${MAKE} doKERNEL KERNEL=${kernel}
	rm -rf ${.CURDIR}/../sys/compile/${kernel}
	ln -f ${RD}/kernels/${kernel} ${RD}/trees/bin/kernel.${kernel}
.endfor
	touch release.3

# Make and install the three crunched binaries which live on the floppies.
# You are not supposed to like this :-)
#
# NB: the "RELEASE_BUILD_FIXIT" magic prevents vi from including the
# Tcl and Perl APIs.  See also /usr/src/usr.bin/vi/Makefile.
release.4:
	cd ${.CURDIR}/sysinstall && make depend all
	rm -rf ${RD}/crunch
	mkdir -p ${RD}/crunch
	export RELEASE_BUILD_FIXIT=noway ; \
	for j in ${WHICH_CRUNCH} ; do \
		rm -rf $${j}_crunch && \
		mkdir $${j}_crunch && \
		( cd $${j}_crunch && \
		crunchgen ${.CURDIR}/$${j}_crunch.conf && \
		${MAKE} -f $${j}_crunch.mk all NOCRYPT=yes \
		    "CFLAGS=${CFLAGS} -DCRUNCHED_BINARY") && \
		mv $${j}_crunch/$${j}_crunch ${RD}/crunch/$${j} && \
		true || { rm -rf $${j}_crunch ; false ; } ; \
	done
	touch release.4

#
# --==## Fix up the distributions. ##==--
#
release.5:
	# Handle some grief caused by the munition braindeadness.
	for i in sbin/init bin/ed ; do \
		( cd ${.CURDIR}/../$$i; \
		  make -DNOCRYPT clean all distribute DISTDIR=${RD}/trees ) ; \
	done

	# Move all the manpages out to their own dist
	if [ -d ${RD}/trees/bin/usr/share/man ] ; then \
		( cd ${RD}/trees/bin/usr/share/man; \
		find . | cpio -dumpl ${RD}/trees/manpages/usr/share/man ) && \
		rm -rf ${RD}/trees/bin/usr/share/man; \
	fi
	# Create the catpages
	if [ -d ${RD}/trees/manpages/usr/share/man ] ; then \
		su -m man -c 'catman ${RD}/trees/manpages/usr/share/man'; \
		( cd ${RD}/trees/manpages/usr/share/man; \
		find cat* | cpio -dumpl ${RD}/trees/catpages/usr/share/man ) && \
		rm -rf ${RD}/trees/manpages/usr/share/man/cat*; \
	fi
	# Move all the games out to their own dist
	if [ -d ${RD}/trees/bin/usr/games ] ; then \
		tar -cf - -C ${RD}/trees/bin/usr/games . | \
			tar -xf - -C ${RD}/trees/games/usr/games && \
		rm -rf ${RD}/trees/bin/usr/games; \
	fi
	if [ -d ${RD}/trees/bin/usr/share/games ] ; then \
		tar -cf - -C ${RD}/trees/bin/usr/share/games . | \
			tar -xf - -C ${RD}/trees/games/usr/share/games && \
		rm -rf ${RD}/trees/bin/usr/share/games; \
	fi
	if [ -d ${RD}/trees/bin/var/games ] ; then \
		tar -cf - -C ${RD}/trees/bin/var/games . | \
			tar -xf - -C ${RD}/trees/games/var/games && \
		rm -rf ${RD}/trees/bin/var/games; \
	fi
	# Move the dict out to their own dist
	if [ -d ${RD}/trees/bin/usr/share/dict ] ; then \
		tar -cf - -C ${RD}/trees/bin/usr/share/dict . | \
			tar -xf - -C ${RD}/trees/dict/usr/share/dict && \
		rm -rf ${RD}/trees/bin/usr/share/dict; \
	fi
	for i in airport birthtoken flowers na.phone zipcodes ; do \
		if [ -f ${RD}/trees/bin/usr/share/misc/$$i ] ; then \
			mv ${RD}/trees/bin/usr/share/misc/$$i \
				${RD}/trees/dict/usr/share/misc; \
		fi ; \
	done
	# Move the info files out to their own dist
	if [ -d ${RD}/trees/bin/usr/share/info ] ; then \
		tar -cf - -C ${RD}/trees/bin/usr/share/info . | \
			tar -xf - -C ${RD}/trees/info/usr/share/info && \
		rm -rf ${RD}/trees/bin/usr/share/info; \
	fi
	# Move the profiled libraries out to their own dist
	for i in ${RD}/trees/bin/usr/lib/*_p.a ; do \
		if [ -f $$i ] ; then \
			mv $$i \
				${RD}/trees/proflibs/usr/lib ; \
		fi ; \
	done
	# Create symlinks for the MD5-based crypt lib, too.  The
	# automatically created links still point to the DES stuff,
	# which went into its own distribution.
	for i in ${RD}/trees/bin/usr/lib/libscrypt* ; do \
		c=`echo $$i | sed -e 's/libscrypt/libcrypt/'` ; \
		rm -f $$c ; \
		ln -s `basename $$i` $$c ; \
	done

	# Remove some files we can faster regen after installing.
	cd ${RD}/trees/bin/usr/share/misc && rm -f termcap.db vgrindefs.db
	# Remove all the directories we don't need.
	-cd ${RD}/trees && \
		find ${EXTRA_DISTS} -depth -type d -print | xargs rmdir
	touch release.5

#
# --==## Make binary dists ##==--
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
	( cd ${RD}/dists && \
		if [ -f krb/krb.aa ] ; then mv krb/* des && rmdir krb ; fi )
	touch release.6


#
# --==## Make source dists ##==--
#
release.7:
	@cd ${.CURDIR} && $(MAKE) doTARBALL SD=/usr/src \
		TD=src TN=sbase ARG="[A-Z]*"
	@for i in `cd /usr/src && echo [a-z]*` ; do \
		if [ -d /usr/src/$$i ] ; then \
			cd ${.CURDIR} && $(MAKE) doTARBALL \
				TN=`echo s$$i | tr -d '.' | sed 's/usr/u/'` \
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
	( cd ${RD}/dists/src && \
		if [ -f ssecure.aa ] ; then mv ssecure.* ../des ; fi && \
		if [ -f sebones.aa ] ; then mv sebones.* ../des ; fi && \
		if [ -f ssecure.inf ] ; then mv ssecure.inf ../des ; fi && \
		if [ -f sebones.inf ] ; then mv sebones.inf ../des ; fi ; )
	@echo "src distribution is finished."
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

release.8: write_mfs_in_kernel dumpnlist
	rm -rf ${RD}/mfsfd
	mkdir ${RD}/mfsfd
	cd ${RD}/mfsfd && \
		mkdir -p etc dev mnt stand/help
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=boot \
		DIR=${RD}/mfsfd/stand ZIP=false
	( cd ${RD}/trees/bin/dev && \
		ls console tty ttyv0 ttyv1 ttyv2 ttyv3 null zero \
		*[swo]d* cuaa[01] cuaa[23] fd[01] rfd[01] \
		cd0a mcd0a scd0a matcd0a wcd0c rst0 rft0 rwt0 | \
	cpio -dump ${RD}/mfsfd/dev )
	( cd ${RD}/mfsfd/dev && rm -f *[swo]d*[bdefgh] )
	cd ${RD}/trees/bin && ls ${BOOT1} | cpio -dump ${RD}/mfsfd/stand
	echo "nameserver      42/tcp name"	> ${RD}/mfsfd/stand/etc/services
	echo "ftp             21/tcp"		>> ${RD}/mfsfd/stand/etc/services
	echo "domain          53/tcp          nameserver" >> ${RD}/mfsfd/stand/etc/services
	echo "domain          53/udp          nameserver" >> ${RD}/mfsfd/stand/etc/services
	echo "cmd             514/tcp    shell" >> ${RD}/mfsfd/stand/etc/services
	gzip -c ${.CURDIR}/../COPYRIGHT > ${RD}/mfsfd/stand/help/COPYRIGHT.hlp.gz
	-test -f ${.CURDIR}/install.cfg && cp ${.CURDIR}/install.cfg ${RD}/mfsfd
	@echo "Making the regular boot floppy."
	tar --exclude CVS -cf - -C ${.CURDIR}/sysinstall help | \
		tar xvf - -C ${RD}/mfsfd/stand
	@echo "Compressing doc files..."
	@gzip -9 ${RD}/mfsfd/stand/help/*.hlp
	sh -e ${.CURDIR}/doFS.sh ${RD} ${MNT} ${BOOTMFSSIZE} ${RD}/mfsfd \
		${MFSINODE} minimum
	mv fs-image fs-image.std
	mv fs-image.size fs-image.std.size
	cd ${.CURDIR} && ${MAKE} doMFSKERN FSIMAGE=std
	mv ${RD}/floppies/bootstd.flp ${RD}/floppies/boot.flp
	@echo "Regular boot floppy made."
	touch release.8

#
# --==## Create a fixit floppy ##==--
#
release.9:
	rm -rf ${RD}/fixitfd
	mkdir ${RD}/fixitfd
	cd ${RD}/fixitfd && \
		mkdir -p dev stand bin sbin etc mnt mnt1 mnt2 mnt3 mnt4 tmp \
		usr/share/misc
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=fixit \
		DIR=${RD}/fixitfd/stand ZIP=false
	( cd ${RD}/fixitfd/dev && \
		sed -e '/^PATH/s/^/#/' ${RD}/trees/bin/dev/MAKEDEV > MAKEDEV && \
		chmod 755 MAKEDEV && \
		sh MAKEDEV all )
	cp ${RD}/trees/bin/etc/spwd.db ${RD}/trees/bin/etc/group \
		${RD}/trees/bin/etc/protocols ${RD}/fixitfd/etc
	cp ${RD}/trees/bin/usr/share/misc/scsi_modes \
		${RD}/fixitfd/usr/share/misc
	cp ${.CURDIR}/fixit.profile ${RD}/fixitfd/.profile
	cp ${.CURDIR}/fixit.services ${RD}/fixitfd/etc/services
	cp ${.CURDIR}/tar.sh ${RD}/fixitfd/stand/tar
	chmod 555 ${RD}/fixitfd/stand/tar
	sh -e ${.CURDIR}/doFS.sh ${RD} ${MNT} ${FDSIZE} ${RD}/fixitfd \
		${FIXITINODE} ${FDLABEL}
	mv fs-image ${RD}/floppies/fixit.flp
# Do our last minute floppies directory setup in a convenient place.
	cp ${.CURDIR}/floppies/README.TXT ${RD}/floppies/README.TXT
	@(cd ${RD}/floppies; md5 * > CHECKSUM.MD5)
	touch release.9

#
# --==## Setup a suitable ftp-area ##==--
#
ftp.1:
	mkdir -p ${FD}
	cd ${RD} && find floppies -print | cpio -dumpl ${FD}
	cd ${RD}/dists && find . -print | cpio -dumpl ${FD}
	cp ${.CURDIR}/ABOUT.TXT ${FD}/ABOUT.TXT
	cp ${.CURDIR}/sysinstall/help/readme.hlp ${FD}/README.TXT
	cp ${.CURDIR}/sysinstall/help/hardware.hlp ${FD}/HARDWARE.TXT
	cp ${.CURDIR}/sysinstall/help/install.hlp ${FD}/INSTALL.TXT
	cp ${.CURDIR}/sysinstall/help/relnotes.hlp ${FD}/RELNOTES.TXT
	echo "CD_VERSION = ${BUILDNAME}" > ${FD}/cdrom.inf

#
# --==## Setup a suitable cdrom-area ##==--
#
cdrom.1:
	mkdir -p ${CD_DISC1} ${CD_DISC2}
	cd ${RD} && find floppies -print | cpio -dumpl ${CD_DISC1}
	cd ${RD}/dists && find . -print | cpio -dumpl ${CD_DISC1}
	ln -f ${RD}/kernels/MFSKERNEL.std ${CD_DISC1}/kernel
	ln -f ${RD}/trees/bin/usr/mdec/fbsdboot.exe ${CD_DISC1}
	for i in ${DISTRIBUTIONS} ; \
	do \
		if [ -d ${RD}/trees/$${i} ] ; then \
			chflags -R noschg ${RD}/trees/$${i} ; \
			( cd ${RD}/trees/$${i} && \
			find . -depth -print | cpio -dumpl ${CD_DISC2} ) ; \
		fi \
	done
	rm -f ${CD_DISC2}/.profile
	cp ${.CURDIR}/fixit.profile ${CD_DISC2}/.profile
	echo "CD_VERSION = ${BUILDNAME}" > ${CD_DISC1}/cdrom.inf
	echo "CD_VERSION = ${BUILDNAME}" > ${CD_DISC2}/cdrom.inf
	cp ${.CURDIR}/ABOUT.TXT ${CD_DISC1}/ABOUT.TXT
	cp ${.CURDIR}/sysinstall/help/readme.hlp ${CD_DISC1}/README.TXT
	cp ${.CURDIR}/sysinstall/help/hardware.hlp ${CD_DISC1}/HARDWARE.TXT
	cp ${.CURDIR}/sysinstall/help/install.hlp ${CD_DISC1}/INSTALL.TXT
	cp ${.CURDIR}/sysinstall/help/relnotes.hlp ${CD_DISC1}/RELNOTES.TXT
.if !defined(NOPORTS)
	ln -s ../ports ${CD_DISC2}/usr/ports
.endif
#
# --==## And finally, create the ports distribution & tarball ##==--
#
# NOTE: Both ftp.1 and cdrom.1 must be run before this target or it will die
#       a quick and gruesome death.  We also expect the release engineer to
#       manually populate ports/distfiles on the 2nd CDROM.

ports.1:
	tar -cBf - -C /usr ports | tar xBpf - -C ${CD_DISC2} && \
		mkdir -p ${CD_DISC1}/ports && tar -czf ${CD_DISC1}/ports/ports.tgz -C /usr ports && \
		cp ${.CURDIR}/scripts/ports-install.sh ${CD_DISC1}/ports/install.sh && \
		(cd ${CD_DISC1}/ports; md5 * > CHECKSUM.MD5) &&
		tar -cBf - -C ${CD_DISC1} ports | tar -xBf - -C ${FD}


# Various "subroutine" and other supporting targets.

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
	rm -rf ${RD}/dists/${TD}/${TN}*
	mkdir -p ${RD}/dists/${TD}
	( cd ${SD} && \
		tn=`echo ${TN} | tr '[A-Z]' '[a-z]' | cut -c1-8` && \
		echo rolling ${TD}/$$tn tarball &&\
		tar --exclude CVS --exclude obj --exclude BOOTMFS -cf - ${ARG} | \
		${ZIPNSPLIT} ${RD}/dists/${TD}/$$tn. && \
		sh ${.CURDIR}/info.sh ${RD}/dists/${TD}/$$tn > \
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

doRELEASE:  release.1 release.2 release.3 release.4 release.5 release.6 \
		release.7 release.8 release.9
.if defined(NOPORTS)
	cd ${.CURDIR} && ${MAKE} cdrom.1 ftp.1
.else
	cd ${.CURDIR} && ${MAKE} cdrom.1 ftp.1 ports.1
.endif
	@echo "Release done"

floppies:
	cd ${.CURDIR} && ${MAKE} boot.flp
	cd ${.CURDIR} && ${MAKE} fixit.flp
	cd ${RD} && find floppies -print | cpio -dumpl ${FD}

boot.flp:
	rm -f release.4 release.8
	cd ${.CURDIR} && ${MAKE} release.4 release.8 WHICH_CRUNCH=boot

fixit.flp:
	rm -f release.4 release.9
	cd ${.CURDIR} && ${MAKE} release.4 release.9 WHICH_CRUNCH=fixit

write_mfs_in_kernel:	${.CURDIR}/write_mfs_in_kernel.c
	${CC} ${CFLAGS} -o write_mfs_in_kernel ${.CURDIR}/write_mfs_in_kernel.c

dumpnlist:	${.CURDIR}/dumpnlist.c
	${CC} ${CFLAGS} -o dumpnlist ${.CURDIR}/dumpnlist.c

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
	if ${ZIP} ; then \
		gzip -9 < ${RD}/crunch/${CRUNCH} > ${DIR}/${CRUNCH}_crunch ; \
	else \
		ln -f ${RD}/crunch/${CRUNCH} ${DIR}/${CRUNCH}_crunch ; \
	fi
	chmod 555 ${DIR}/${CRUNCH}_crunch
	for i in `crunchgen -l ${.CURDIR}/${CRUNCH}_crunch.conf` ; do \
		ln -f ${DIR}/${CRUNCH}_crunch ${DIR}/$$i ; \
	done

#
# --==## Compile a kernel by name ${KERNEL} ##==--
#
# We don't erase the sys/compile/${KERNEL} directory, since somebody
# may want to reuse it (release.8 presently)
#
doKERNEL:
	rm -f ${RD}/kernels/${KERNEL}
	cd ${.CURDIR}/../sys/i386/conf && config -n ${KERNEL}
	cd ${.CURDIR}/../sys/compile/${KERNEL} && \
			make  depend && \
			make  kernel && \
			cp kernel ${RD}/kernels/${KERNEL}

#
# --==## Put a filesystem into a BOOTMFS kernel ##==--
#
doMFSKERN:
	@rm -f ${RD}/kernels/BOOTMFS.${FSIMAGE}
	@rm -f /sys/compile/BOOTMFS/mfs_vfsops.o
	@cd ${.CURDIR}/../sys/i386/conf && \
	  sed	-e '/SYSV/d' \
		-e '/pty/d' \
		-e '/PROCFS/d' \
		-e '/KTRACE/d' \
		-e 's/GENERIC/BOOTMFS/g' \
		-e '/maxusers/s/10/4/' < GENERIC > BOOTMFS && \
	  echo "options  MFS" >> BOOTMFS && \
	  echo "options  NFS_NOSERVER" >> BOOTMFS && \
	  echo 'options  "MAXCONS=4"' >> BOOTMFS && \
	  echo "options  USERCONFIG_BOOT" >> BOOTMFS
	@echo "options \"MFS_ROOT=`cat fs-image.${FSIMAGE}.size`\"" >> \
	  ${.CURDIR}/../sys/i386/conf/BOOTMFS
	cd ${.CURDIR} && ${MAKE} doKERNEL KERNEL=BOOTMFS
	@rm -rf ${RD}/boot.${FSIMAGE}
	@mkdir  ${RD}/boot.${FSIMAGE}
	@mv ${RD}/kernels/BOOTMFS ${RD}/kernels/BOOTMFS.${FSIMAGE}
	@cp ${RD}/kernels/BOOTMFS.${FSIMAGE} ${RD}/boot.${FSIMAGE}/kernel
	@vnconfig /dev/vn0 fs-image.${FSIMAGE}
	@mkdir -p /tmp/mnt_xx
	@mount /dev/vn0 /tmp/mnt_xx
	./dumpnlist ${RD}/boot.${FSIMAGE}/kernel > /tmp/mnt_xx/stand/symbols
	@umount /tmp/mnt_xx
	@vnconfig -u /dev/vn0
	@rmdir /tmp/mnt_xx
	./write_mfs_in_kernel ${RD}/boot.${FSIMAGE}/kernel \
		fs-image.${FSIMAGE}
	kzip -v ${RD}/boot.${FSIMAGE}/kernel
	@mv ${RD}/boot.${FSIMAGE}/kernel ${RD}/kernels/MFSKERNEL.${FSIMAGE}
	@mv ${RD}/boot.${FSIMAGE}/kernel.kz ${RD}/boot.${FSIMAGE}/kernel
	@rm -f ${RD}/floppies/boot${FSIMAGE}.flp
	sh -e ${.CURDIR}/doFS.sh ${RD} ${MNT} ${FDSIZE} ${RD}/boot.${FSIMAGE} \
		${BOOTINODE} ${FDLABEL}
	mv fs-image ${RD}/floppies/boot${FSIMAGE}.flp

.include <bsd.prog.mk>
