#	$Id: Makefile,v 1.131 1995/04/09 03:47:55 jkh Exp $
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

EXPORT_DISTS=	games manpages proflibs dict info
EXTRA_DISTS=	secure ${EXPORT_DISTS}

CPIO1=	etc/services etc/protocols etc/sysconfig
CPIO2=  scripts/miscfuncs.sh scripts/instdist.sh scripts/netinst.sh \
	scripts/adduser.sh scripts/bininst.sh scripts/setup.sh

FAQS=	README MIRROR.SITES

# define this to use a vn device instead of the floppy drive
# vn driver can still do odd bogosities.  don't use it.
# VNDEVICE=		vn0

# work dir for floppies
MNT=			/mnt

# define FDFORMAT to format physical floppies
FDFORMAT=		yes

FDDEVICE=		fd0
FDCYLS=			80
FDCYLSIZE=		15k
FDLABEL=		fd1200

NEWFSARGS=		-c 80 -b 4096 -f 512 -i 9000 -m 0 -t 0 -u 0 \
				-o space -T ${FDLABEL}

ZIPNSPLIT=		gzip --no-name -9 -c | split -b 240640 -


# Things which will get you into trouble if you change them
DISTRIBUTIONS=		bin ${EXTRA_DISTS}                      
MTREEFILES=		${.CURDIR}/../etc/mtree
RD=			${RELEASEDIR}/stage
FD=			${RELEASEDIR}/ftp
CD=			${RELEASEDIR}/cdrom

release:
.if !defined(CHROOTDIR)
	@echo "To make a release you must set CHROOTDIR" && false
.endif
	-mkdir ${CHROOTDIR}
	chflags -R noschg ${CHROOTDIR}/.
	rm -rf ${CHROOTDIR}/*
	cd ${.CURDIR}/../etc ; ${MAKE} distrib-dirs DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/../etc ; ${MAKE} distribution DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/.. ; ${MAKE} -k install DESTDIR=${CHROOTDIR}
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
	cd ${CHROOTDIR}/usr ; cvs co -P src
.else
	cd ${CHROOTDIR}/usr ; cvs export -r ${RELEASETAG} src
.endif
	# To be used in development...
	#find ${.CURDIR} -print | cpio -dumpv ${CHROOTDIR}
	chmod 755 ${CHROOTDIR}/mk
	chroot ${CHROOTDIR} /mk

clean:
	rm -rf cpio_crunch boot_crunch release.[0-9] release.1[0]

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
	mkdir ${RD}/cpiofd/stand/scripts
	mkdir ${RD}/fixitfd
	mkdir ${RD}/fixitfd/stand
	mkdir ${RD}/fixitfd/bin
	mkdir ${RD}/fixitfd/sbin
	mkdir ${RD}/fixitfd/mnt
	mkdir ${RD}/fixitfd/etc
	mkdir ${RD}/floppies
	mkdir ${RD}/kernels
	mkdir ${RD}/trees
	mkdir ${RD}/trees/bin
	mkdir ${RD}/dists
	mkdir ${RD}/dists/bin
	cd ${.CURDIR}/../etc && make distrib-dirs DESTDIR=${RD}/trees/bin
	for i in ${EXTRA_DISTS} ; do \
		mkdir ${RD}/trees/$$i ; \
		mkdir ${RD}/dists/$$i ; \
		mtree -deU -f ${MTREEFILES}/BSD.root.dist \
		    -p ${RD}/trees/$$i > /dev/null ; \
		mtree -deU -f ${MTREEFILES}/BSD.usr.dist \
		    -p ${RD}/trees/$$i/usr > /dev/null ; \
		mtree -deU -f ${MTREEFILES}/BSD.include.dist \
		    -p ${RD}/trees/$$i/usr/include > /dev/null ; \
		mtree -deU -f ${MTREEFILES}/BSD.var.dist \
		    -p ${RD}/trees/$$i/var > /dev/null ; \
	done
	touch release.1

# Install the system into the various distributions.
release.2:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	cd ${.CURDIR}/.. ; make distribute DISTDIR=${RD}/trees
	chflags -R noschg ${RD}/trees
	touch release.2

# Make and install a couple of kernels we need.
release.3:
	@cd ${.CURDIR} && $(MAKE) ckRELEASEDIR
	cd ${.CURDIR}/../sys/i386/conf && \
		sed 's/GENERIC/BOOTCDROM/g' GENERIC > BOOTCDROM
	for i in GENERIC BOOTFLP BOOTCDROM ; do \
		cd ${.CURDIR}/../sys/i386/conf && \
			config $$i && \
		cd ${.CURDIR}/../sys/compile/$$i && \
			make depend && \
			make kernel && \
			cp kernel ${RD}/kernels/$$i ; \
	done
	rm -f ${.CURDIR}/../sys/i386/conf/BOOTCDROM
	ln -f ${RD}/kernels/GENERIC ${RD}/trees/bin/kernel.GENERIC
	ln -f ${RD}/kernels/GENERIC ${RD}/cpiofd/kernel
	cp ${RD}/kernels/BOOTFLP ${RD}/bootfd/kernel
	strip ${RD}/bootfd/kernel
	cp ${RD}/kernels/BOOTFLP ${RD}/fixitfd/kernel
	strip ${RD}/fixitfd/kernel
	touch release.3

# Make and install the three crunched binaries which live on the floppies.
# You are not supposed to like this :-)
release.4:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	for j in boot cpio fixit; do \
		rm -rf $${j}_crunch ; \
		mkdir $${j}_crunch ; \
		( cd $${j}_crunch ; \
		crunchgen ${.CURDIR}/$${j}_crunch.conf ; \
		${MAKE} -f $${j}_crunch.mk objs exe NOCRYPT=yes ) ; \
		if [ $$j = cpio ] ; then \
			cat < $${j}_crunch/$${j}_crunch \
				> ${RD}/$${j}fd/stand/$${j}_crunch ; \
		else \
			gzip -9 < $${j}_crunch/$${j}_crunch \
				> ${RD}/$${j}fd/stand/$${j}_crunch ; \
		fi ; \
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
	if [ -d ${RD}/trees/bin/usr/share/man ] ; then \
		rm -rf ${RD}/trees/manpages/usr/share/man ;\
		mv ${RD}/trees/bin/usr/share/man \
			${RD}/trees/manpages/usr/share/man ;\
	fi
	if [ -d ${RD}/trees/bin/usr/games ] ; then \
		rm -rf ${RD}/trees/games/usr/games ;\
		mv ${RD}/trees/bin/usr/games \
			${RD}/trees/games/usr/games ;\
	fi
	if [ -d ${RD}/trees/bin/usr/share/games ] ; then \
		rm -rf ${RD}/trees/games/usr/share/games ;\
		mv ${RD}/trees/bin/usr/share/games \
			${RD}/trees/games/usr/share/games ;\
	fi
	if [ -d ${RD}/trees/bin/var/games ] ; then \
		rm -rf ${RD}/trees/games/var/games ;\
		mv ${RD}/trees/bin/var/games \
			${RD}/trees/games/var/games ;\
	fi
	if [ -d ${RD}/trees/bin/usr/share/dict ] ; then \
		rm -rf ${RD}/trees/dict/usr/share/dict ;\
		mv ${RD}/trees/bin/usr/share/dict \
			${RD}/trees/dict/usr/share/dict ;\
	fi
	for i in airport birthtoken flowers na.phone zipcodes ; do \
		if [ -f ${RD}/trees/bin/usr/share/misc/$$i ] ; then \
			mv ${RD}/trees/bin/usr/share/misc/$$i \
				${RD}/trees/dict/usr/share/misc ; \
		fi ; \
	done
	-mv ${RD}/trees/bin/usr/share/info/* \
		${RD}/trees/info/usr/share/info
	for i in ${RD}/trees/bin/usr/lib/*_p.a ; do \
		if [ -f $$i ] ; then \
			mv $$i \
				${RD}/trees/proflibs/usr/lib ; \
		fi ; \
	done
	cd ${RD}/trees/bin/usr/share/misc && rm -f termcap.db vgrindefs.db
	-cd ${RD}/trees && \
		find ${EXTRA_DISTS} -depth -type d -print | xargs rmdir
	touch release.5

# Complete the bootfd 
release.6:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	( cd ${RD}/trees/bin/dev ; \
		ls console tty ttyv1 null zero \
		sd[0123][a-h] wd[0123][a-h] fd[01] \
		rsd[0123][a-h] rwd[0123][a-h] rfd[01] | \
	cpio -dump ${RD}/bootfd/dev )
	install -c ${.CURDIR}/../COPYRIGHT ${RD}/bootfd
	cd ${RD}/trees/bin/usr/share/FAQ/Text ; \
		install -c diskspace.FAQ ${RD}/bootfd/DISKSPACE.FAQ ;\
		install -c RELNOTES.FreeBSD ${RD}/bootfd ;\
		install -c TROUBLESHOOTING ${RD}/bootfd ;\
		install -c README ${RD}/bootfd/README
	touch ${RD}/bootfd/this_is_boot_flp
	cd ${.CURDIR} ; ${MAKE} doFLOPPY FLOPPY=boot
	touch release.6

# Complete the cpiofd directory
release.7:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	cd ${RD}/trees/bin ; ls ${CPIO1} | cpio -dump ${RD}/cpiofd/stand
	cd ${.CURDIR} ; ls ${CPIO2} | cpio -dumpv ${RD}/cpiofd/stand
	( JUST_TELL_ME=please ; . ${.CURDIR}/../sys/conf/newvers.sh ; \
	sed "/DISTNAME=/s/FOOBAR/$$DISTNAME/" \
		< ${RD}/cpiofd/stand/scripts/miscfuncs.sh \
		> ${RD}/cpiofd/stand/scripts/tmp ;\
	)
	mv ${RD}/cpiofd/stand/scripts/tmp \
		${RD}/cpiofd/stand/scripts/miscfuncs.sh
	chmod 755 ${RD}/cpiofd/stand/scripts/*
	cp ${RD}/trees/bin/usr/sbin/tzsetup ${RD}/cpiofd/stand
	( cd ${RD}/cpiofd ; \
		( rm -f OK ; find . -print ; touch OK ; echo OK ) | \
		cpio -H newc -oa | gzip -9 -c | dd conv=osync ) \
		> ${RD}/floppies/cpio.flp
	gzip -1 -c < ${RD}/floppies/cpio.flp > ${RD}/floppies/cpio.flp.gz
	touch release.7

# Make binary dists, notice that secure dist isn't finished yet.
release.8:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	rm -rf ${RD}/dists
	mkdir -p ${RD}/dists
	@for i in ${DISTRIBUTIONS} ; \
	do \
		if [ -d ${RD}/trees/$${i} ] ; then \
			cd ${.CURDIR} ; \
				$(MAKE) doTARBALL SD=${RD}/trees/$${i} \
					TN=$$i TD=$$i ARG="." ; \
			( cd ${RD}/dists/$${i}; \
			sh -e ${.CURDIR}/scripts/mkchecksums.sh ) ; \
			cp ${.CURDIR}/scripts/extract_$${i}.sh \
				${RD}/dists/$${i}/extract.sh;\
			echo "$${i} distribution is finished."; \
		fi ; \
	done                                                 
	touch release.8


# Make source dists, and finish of secure too
release.9:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	@cd ${.CURDIR} ; $(MAKE) doTARBALL SD=/usr/src \
		TD=src TN=sbase ARG="[A-Z]*"
	@for i in `cd /usr/src ; echo [a-z]*` ; do \
		if [ -d /usr/src/$$i -a "$$i" != "CVS" ] ; then \
			cd ${.CURDIR} ; \
			$(MAKE) doTARBALL SD=/usr/src \
				TN=`echo s$$i | tr -d '.' | sed 's/usr/u/'` \
				TD=src ARG="$$i" ; \
		fi ; \
	done
	( cd ${RD}/dists/src && \
	if [ -f ssecure.aa ] ; then mv ssecure.* ../secure ; fi && \
	if [ -f sebones.aa ] ; then mv sebones.* ../secure ; fi && \
	sh -e ${.CURDIR}/scripts/mkchecksums.sh )
	cp ${.CURDIR}/scripts/extract_src.sh ${RD}/dists/src/extract.sh
	if [ -d ${RD}/dists/secure ] ; then \
	( cd ${RD}/dists/secure && sh -e ${.CURDIR}/scripts/mkchecksums.sh );\
	fi
	@echo "src distribution is finished."
	touch release.9

# Complete the fixitfd
release.10:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	( cd ${RD}/trees/bin/dev ; \
		ls console tty ttyv1 null zero \
		sd[0123][a-h] wd[0123][a-h] fd[01] \
		rsd[0123][a-h] rwd[0123][a-h] rfd[01] | \
	cpio -dump ${RD}/fixitfd/dev )
	ln -f ${RD}/fixitfd/stand/init ${RD}/fixitfd/sbin
	ln -f ${RD}/fixitfd/stand/sh ${RD}/fixitfd/bin
	cp ${RD}/trees/bin/usr/mdec/fdboot ${RD}/fixitfd/etc/boot1
	cp ${RD}/trees/bin/usr/mdec/bootfd ${RD}/fixitfd/etc/boot2
	cp ${.CURDIR}/fixit.profile ${RD}/fixitfd/.profile
	echo 'echo /etc/rc'	> ${RD}/fixitfd/etc/rc
	echo 'exit 1'		>> ${RD}/fixitfd/etc/rc
	touch ${RD}/fixitfd/etc/spwd.db
	cd ${.CURDIR} ; ${MAKE} doFLOPPY FLOPPY=fixit
	touch release.10

ftp.1:
	rm -rf ${FD}
	mkdir ${FD}
	cd ${RD} && find floppies -print | cpio -dumpl ${FD}
	cd ${RD}/dists && find . -print | cpio -dumpl ${FD}
	cd ${RD}/trees/bin/usr/share/FAQ/Text && ln -f ${FAQS} ${FD}

cdrom.1:
	rm -rf ${CD}
	mkdir ${CD}
	mkdir ${CD}/filesys
	cd ${RD} && find floppies dists bootfd -print | cpio -dumpl ${CD}
	cd ${CD} && mv bootfd bootcd
	rm -f ${CD}/bootcd/kernel
	ln -f ${RD}/kernels/BOOTCDROM ${CD}/bootcd/kernel
	ln -f ${RD}/trees/bin/usr/mdec/fbsdboot.exe ${CD}/bootcd
	for i in ${DISTRIBUTIONS} ; \
	do \
		if [ -d ${RD}/trees/$${i} ] ; then \
			( cd ${RD}/trees/$${i} && \
			find . -depth -print | cpio -dumpl ${CD}/filesys ) ; \
		fi \
	done

# Various "subroutine" and other supporting targets.

doTARBALL:
.if !defined(SD)
	@echo "SD undefined in doTARBALL" ; exit 1
.endif               
.if !defined(TD)
	@echo "TB undefined in doTARBALL" ; exit 1
.endif               
.if !defined(ARG)
	@echo "ARG undefined in doTARBALL" ; exit 1
.endif               
	rm -rf ${RD}/dists/${TD}/${TN}*
	mkdir -p ${RD}/dists/${TD}
	( cd ${SD} && \
		tn=`echo ${TN} | tr '[A-Z]' '[a-z]' | cut -c1-8` && \
		echo rolling ${TD}/$$tn tarball &&\
		tar --exclude CVS --exclude obj -cf - ${ARG} | \
		${ZIPNSPLIT} ${RD}/dists/${TD}/$$tn. )

doRELEASE:  release.1 release.2 release.3 release.4 release.5 release.6 \
		release.7 release.8 release.9 release.10
	@echo "Release done"

ckRELEASEDIR:
.if !defined(RELEASEDIR)
	@echo "To make a release RELEASEDIR must be defined" && false
.endif               

doFLOPPY:
.if !defined(FLOPPY)
	@echo "FLOPPY undefined in doFLOPPY" ; exit 1
.endif
	rm -f ${RD}/floppies/${FLOPPY}.tmp ${RD}/floppies/${FLOPPY}.flp
.if defined(VNDEVICE)
	if [ ! -b /dev/${VNDEVICE} -o ! -c /dev/r${VNDEVICE} ] ; then \
		cd /dev && sh MAKEDEV ${VNDEVICE} ; fi
	@umount /dev/${VNDEVICE} 2>/dev/null || true
	@umount ${MNT} 2>/dev/null || true
	@vnconfig -u /dev/r${VNDEVICE} 2>/dev/null || true
	dd of=${RD}/floppies/${FLOPPY}.tmp if=/dev/zero \
		bs=${FDCYLSIZE} count=${FDCYLS} 2>/dev/null
	sync && sleep 5
	vnconfig -c /dev/r${VNDEVICE} ${RD}/floppies/${FLOPPY}.tmp
	disklabel -w -r -B \
		-b ${RD}/trees/bin/usr/mdec/fdboot \
		-s ${RD}/trees/bin/usr/mdec/bootfd \
		/dev/r${VNDEVICE} ${FDLABEL}
	newfs ${NEWFSARGS} /dev/r${VNDEVICE}
	mount /dev/${VNDEVICE} ${MNT}
	cd ${RD}/${FLOPPY}fd ; find . -print | cpio -dump ${MNT}
	umount ${MNT}
	fsck  /dev/r${VNDEVICE} < /dev/null
.else
	-umount /dev/${FDDEVICE}
	-umount ${MNT}
.if !defined(FDFORMAT)
	echo y | fdformat /dev/r${FDDEVICE}
.endif
	disklabel -w -r -B \
		-b ${RD}/trees/bin/usr/mdec/fdboot \
		-s ${RD}/trees/bin/usr/mdec/bootfd \
		/dev/r${FDDEVICE} ${FDLABEL}
	newfs ${NEWFSARGS} /dev/r${FDDEVICE}
	mount /dev/${FDDEVICE} ${MNT}
	cd ${RD}/${FLOPPY}fd ; find . -print | cpio -dump ${MNT}
	umount ${MNT}
	fsck -n /dev/r${FDDEVICE} < /dev/null
	dd if=/dev/r${FDDEVICE} of=${RD}/floppies/${FLOPPY}.tmp \
		bs=${FDCYLSIZE} count=${FDCYLS}
.endif
	mv ${RD}/floppies/${FLOPPY}.tmp ${RD}/floppies/${FLOPPY}.flp
	gzip -9 -c < ${RD}/floppies/${FLOPPY}.flp \
		> ${RD}/floppies/${FLOPPY}.flp.gz

.include <bsd.prog.mk>
