#	$Id: Makefile,v 1.110 1995/02/26 20:45:34 phk Exp $
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

# Things which without too much trouble can be considered variables
EXTRADISTRIBUTIONS=	secure games manpages proflibs dict info
CPIO1=			etc/services etc/protocols
CPIO2=  scripts/miscfuncs.sh scripts/instdist.sh scripts/netinst.sh \
	scripts/adduser.sh scripts/bininst.sh scripts/setup.sh

FDDEVICE=		fd0
FDCYLS=			80
FDCYLSIZE=		15k
FDLABEL=		fd1200
MNT=			/mnt

ZIPNSPLIT=		gzip --no-name -9 -c | split -b 240640 -


# Things which will get you into trouble if you change them
DISTRIBUTIONS=		bin ${EXTRADISTRIBUTIONS}                      
MTREEFILES=		${.CURDIR}/../etc/mtree
RD=			${RELEASEDIR}/stage

release:
.if !defined(CHROOTDIR)
	@echo "To make a release you must set CHROOTDIR" && false
.endif
	-mkdir ${CHROOTDIR}
	chflags -R noschg ${CHROOTDIR}/.
	rm -rf ${CHROOTDIR}/*
	cd ${.CURDIR}/../etc ; ${MAKE} distrib-dirs DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/../etc ; ${MAKE} distribution DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/.. ; ${MAKE} install DESTDIR=${CHROOTDIR}
	echo "#!/bin/sh"			> ${CHROOTDIR}/mk
	echo "set -ex"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src"			>> ${CHROOTDIR}/mk
	echo "make world"			>> ${CHROOTDIR}/mk
	echo "cd sbin/sysinstall"		>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "cd ../../release"			>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make doRELEASE"			>> ${CHROOTDIR}/mk
	echo "RELEASEDIR=/R"			>> ${CHROOTDIR}/etc/make.conf
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr ; cvs co src -P
.else
	cd ${CHROOTDIR}/usr ; cvs export -P -r ${RELEASETAG} src
.endif
	chmod 755 ${CHROOTDIR}/mk
	chroot ${CHROOTDIR} /mk

clean:
	rm -rf cpio_crunch boot_crunch release.[1-9]

# Clean out ${RELEASEDIR} and make the directory structure.
release.1:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	-mkdir ${RELEASEDIR}
	chflags -R noschg ${RELEASEDIR}/.
	rm -rf ${RELEASEDIR}/*
	mkdir ${RD}
	mkdir ${RD}/bootfd
	mkdir ${RD}/bootfd/dev
	mkdir ${RD}/bootfd/mnt
	mkdir ${RD}/bootfd/stand
	mkdir ${RD}/cpiofd
	mkdir ${RD}/cpiofd/stand
	mkdir ${RD}/floppies
	mkdir ${RD}/dists
	mkdir ${RD}/dists/bin
	mkdir ${RD}/tarballs
	mkdir ${RD}/tarballs/bin
	cd ${.CURDIR}/../etc && make distrib-dirs DESTDIR=${RD}/dists/bin
	for i in ${EXTRADISTRIBUTIONS} ; do \
		mkdir ${RD}/dists/$$i ; \
		mkdir ${RD}/tarballs/$$i ; \
		mtree -deU -f ${MTREEFILES}/BSD.root.dist \
		    -p ${RD}/dists/$$i > /dev/null ; \
		mtree -deU -f ${MTREEFILES}/BSD.usr.dist \
		    -p ${RD}/dists/$$i/usr > /dev/null ; \
		mtree -deU -f ${MTREEFILES}/BSD.var.dist \
		    -p ${RD}/dists/$$i/var > /dev/null ; \
	done
	touch release.1

# Install the system into the various distributions.
release.2:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	cd ${.CURDIR}/.. ; make distribute DISTDIR=${RD}/dists
	touch release.2

# Make and install a couple of kernels we need.
release.3:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	cd ${.CURDIR}/../sys/i386/conf ; \
		config GENERIC ; \
		config BOOTFLP
	cd ${.CURDIR}/../sys/compile/GENERIC ; \
		make depend ; \
		make kernel ; \
		install -c kernel ${RD}/dists/bin/kernel.GENERIC
	ln  -f ${RD}/dists/bin/kernel.GENERIC \
		${RD}/cpiofd/kernel
	cd ${.CURDIR}/../sys/compile/BOOTFLP ; \
		make depend ; \
		make kernel ; \
		install -s -c kernel ${RD}/bootfd/kernel
	touch release.3

# Make and install the two crunched binaries which live on the floppies.
# You are not supposed to like this :-)
release.4:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	for j in boot cpio ; do \
		rm -rf $${j}_crunch ; \
		mkdir $${j}_crunch ; \
		( cd $${j}_crunch ; \
		crunchgen ${.CURDIR}/$${j}_crunch.conf ; \
		${MAKE} -f $${j}_crunch.mk objs exe NOCRYPT=yes ) ; \
		gzip -9 < $${j}_crunch/$${j}_crunch \
			> ${RD}/$${j}fd/stand/$${j}_crunch ; \
		rm -rf $${j}_crunch ; \
		chmod 555 ${RD}/$${j}fd/stand/$${j}_crunch ; \
		for i in `crunchgen -l ${.CURDIR}/$${j}_crunch.conf` ; do \
			ln -f ${RD}/$${j}fd/stand/$${j}_crunch \
				${RD}/$${j}fd/stand/$$i ; \
		done ; \
	done
	touch release.4

# Fix up the distributions.
release.5:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	if [ -d ${RD}/dists/bin/usr/share/man ] ; then \
		rm -rf ${RD}/dists/manpages/usr/share/man ;\
		mv ${RD}/dists/bin/usr/share/man \
			${RD}/dists/manpages/usr/share/man ;\
	fi
	if [ -d ${RD}/dists/bin/usr/games ] ; then \
		rm -rf ${RD}/dists/games/usr/games ;\
		mv ${RD}/dists/bin/usr/games \
			${RD}/dists/games/usr/games ;\
	fi
	if [ -d ${RD}/dists/bin/usr/share/games ] ; then \
		rm -rf ${RD}/dists/games/usr/share/games ;\
		mv ${RD}/dists/bin/usr/share/games \
			${RD}/dists/games/usr/share/games ;\
	fi
	if [ -d ${RD}/dists/bin/var/games ] ; then \
		rm -rf ${RD}/dists/games/var/games ;\
		mv ${RD}/dists/bin/var/games \
			${RD}/dists/games/var/games ;\
	fi
	if [ -d ${RD}/dists/bin/usr/share/dict ] ; then \
		rm -rf ${RD}/dists/dict/usr/share/dict ;\
		mv ${RD}/dists/bin/usr/share/dict \
			${RD}/dists/dict/usr/share/dict ;\
	fi
	for i in airport birthtoken flowers na.phone zipcodes ; do \
		if [ -f ${RD}/dists/bin/usr/share/misc/$$i ] ; then \
			mv ${RD}/dists/bin/usr/share/misc/$$i \
				${RD}/dists/dict/usr/share/misc ; \
		fi ; \
	done
	for i in ${RD}/dists/bin/usr/lib/*_p.a ; do \
		if [ -f $$i ] ; then \
			mv $$i \
				${RD}/dists/proflibs/usr/lib ; \
		fi ; \
	done
	-cd ${RD}/dists ; \
		find ${EXTRADISTRIBUTIONS} -depth -type d -print | xargs rmdir

	touch release.5

# Complete the bootfd 
release.6:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	( cd ${RD}/dists/bin/dev ; \
		ls console tty ttyv1 null zero \
		sd[0123][a-h] wd[0123][a-h] fd[01] \
		rsd[0123][a-h] rwd[0123][a-h] rfd[01] | \
	cpio -dump ${RD}/bootfd/dev )
	install -c ${.CURDIR}/../COPYRIGHT ${RD}/bootfd
	cd ${RD}/dists/bin/usr/share/FAQ ; \
		install -c diskspace.FAQ ${RD}/bootfd/DISKSPACE.FAQ ;\
		install -c RELNOTES.FreeBSD ${RD}/bootfd ;\
		install -c TROUBLESHOOTING ${RD}/bootfd ;\
		install -c README-2.0 ${RD}/bootfd/README
	touch ${RD}/bootfd/this_is_boot_flp
.if !defined(VNDEVICE)
	-umount /dev/${FDDEVICE}
	-umount ${MNT}
	echo y | fdformat /dev/r${FDDEVICE}
	disklabel -w -r -B \
		-b ${RD}/dists/bin/usr/mdec/fdboot \
		-s ${RD}/dists/bin/usr/mdec/bootfd \
		/dev/r${FDDEVICE} ${FDLABEL}
	newfs -c 80 -b 4096 -f 512 -i 9000 -m 0 -t 0 -u 0 \
		-o space -T ${FDLABEL} /dev/r${FDDEVICE}
	mount -o async /dev/${FDDEVICE} ${MNT}
	cd ${RD}/bootfd ; find . -print | cpio -dumpv ${MNT}
	sync
	umount ${MNT}
	fsck /dev/r${FDDEVICE}
	dd if=/dev/r${FDDEVICE} of=${RD}/floppies/boot.tmp \
		bs=${FDCYLSIZE} count=${FDCYLS}
	mv ${RD}/floppies/boot.tmp ${RD}/floppies/boot.flp
	gzip -9 -c < ${RD}/floppies/boot.flp > ${RD}/floppies/boot.flp.gz
.endif
	touch release.6

# Complete the cpiofd directory
# XXX a lot of stuff is missing here...
release.7:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	cd ${RD}/dists/bin ; ls ${CPIO1} | cpio -dump ${RD}/cpiofd
	cd ${.CURDIR} ; ls ${CPIO2} | cpio -dumpv ${RD}/cpiofd
	chmod 755 ${RD}/cpiofd/scripts/*
	cp ${RD}/dists/bin/usr/sbin/tzsetup ${RD}/cpiofd
	( cd ${RD}/cpiofd ; \
		( rm -f OK ; find . -print ; touch OK ; echo OK ) | \
		cpio -H newc -oa | gzip -9 -c | dd conv=osync ) \
		> ${RD}/floppies/cpio.flp
	gzip -1 -c < ${RD}/floppies/cpio.flp > ${RD}/floppies/cpio.flp.gz
	touch release.7

# Make binary tarballs
release.8:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	rm -rf ${RD}/tarballs
	mkdir -p ${RD}/tarballs
	@for i in ${DISTRIBUTIONS} ; \
	do \
		if [ -d ${RD}/dists/$${i} ] ; then \
			cd ${.CURDIR} ; \
				$(MAKE) doTARBALL SD=${RD}/dists/$${i} \
					TD=$$i ARG="." ; \
			( cd ${RD}/tarballs/$${i}; \
			sh -e ${.CURDIR}/scripts/mkchecksums.sh ) ; \
			cp ${.CURDIR}/scripts/extract_$${i}.sh \
				${RD}/tarballs/$${i}/extract.sh;\
			echo "$${i} distribution is finished."; \
		fi ; \
	done                                                 
	touch release.8


# Make source tarballs
release.9:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	@cd ${.CURDIR} ; $(MAKE) doTARBALL SD=/usr/src TD=src/base ARG="[A-Z]*"
	@for i in bin etc games gnu include lib libexec release sbin \
		secure lkm eBones share sys usrbin usrsbin ; do \
		if [ -d /usr/src/$$i ] ; then \
			cd ${.CURDIR} ; \
				$(MAKE) doTARBALL SD=/usr/src \
					TD=src/$$i ARG="$$i" ; \
		fi ; \
	done
	( cd ${RD}/tarballs/src; \
	sh -e ${.CURDIR}/scripts/mkchecksums.sh ) ; \
	cp ${.CURDIR}/scripts/extract_src.sh \
		${RD}/tarballs/src/extract.sh;\
	echo "src distribution is finished."; \
	touch release.9

# Various "subroutine" and other supporting targets.

doTARBALL:
.if !defined(SD)
	@echo "SD undefined in doTARBALL" ; exit 1
.endif               
.if !defined(TD)
	@echo "TB undefined in doTARBALL" ; exit 1
.endif               
.if !defined(ARG)
	@echo "ARGundefined in doTARBALL" ; exit 1
.endif               
	rm -rf ${RD}/tarballs/${TD}
	mkdir -p ${RD}/tarballs/${TD}
	( cd ${SD} ; \
		echo rolling ${TD} tarball ;\
		tar --exclude CVS -cf - ${ARG} | \
		${ZIPNSPLIT} ${RD}/tarballs/${TD}/`basename ${TD}`. )

doRELEASE:  release.1 release.2 release.3 release.4 release.5 release.6 \
		release.7 release.8 release.9
	@echo "Release done"

ckRELEASEDIR:
.if !defined(RELEASEDIR)
	@echo "To make a release RELEASEDIR must be defined" && false
.endif               

.include <bsd.prog.mk>
