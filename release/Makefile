#	$Id: Makefile,v 1.168 1995/05/22 06:25:26 jkh Exp $
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

# SET THIS !!!
#BUILDNAME=SOME_RANDOM_BUILD

# If this is a RELEASE, then set
#RELEASETAG=something

# Things which without too much trouble can be considered variables

EXPORT_DISTS=	games manpages proflibs dict info
EXTRA_DISTS=	des ${EXPORT_DISTS}
ALL_DISTS=	bin ${EXTRA_DISTS}

CPIO1=	etc/services etc/protocols etc/sysconfig

FAQS=	README MIRROR.SITES

# define this to use a vn device instead of the floppy drive
VNDEVICE=		vn0

# which floppy to use, if VNDEVICE isn't defined.
FDDEVICE=		fd0

# define FDFORMAT to format physical floppies
FDFORMAT=		yes

# mountpoint for filesystems.
MNT=			/mnt

# other floppy parameters.
FDCYLS=			80
FDCYLSIZE=		15k
FDLABEL=		fd1200

NEWFSARGS=		-c 80 -b 4096 -f 512 -i 4300 -m 0 -t 0 -u 0 \
				-o space -T ${FDLABEL}

ZIPNSPLIT=		gzip --no-name -9 -c | split -b 240640 -


# Things which will get you into trouble if you change them
DISTRIBUTIONS=		bin ${EXTRA_DISTS}                      
MTREEFILES=		${.CURDIR}/../etc/mtree
RD=			${RELEASEDIR}/stage
FD=			${RELEASEDIR}/ftp
CD=			${RELEASEDIR}/cdrom

.if !defined(WHICH_CRUNCH)
WHICH_CRUNCH=	boot cpio fixit
.endif

release:
.if !defined(CHROOTDIR) || !defined(BUILDNAME)
	@echo "To make a release you must set CHROOTDIR and BUILDNAME" && false
.endif
.if exists(${CHROOTDIR})
	chflags -R noschg ${CHROOTDIR}/.
	rm -rf ${CHROOTDIR}/*
.else
	mkdir ${CHROOTDIR}
.endif
	cd ${.CURDIR}/../etc ; ${MAKE} distrib-dirs DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/../etc ; ${MAKE} distribution DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/.. ; ${MAKE} install DESTDIR=${CHROOTDIR}
	echo "#!/bin/sh"			> ${CHROOTDIR}/mk
	echo "set -ex"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src"			>> ${CHROOTDIR}/mk
	echo "make world"			>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release/libdisk"	>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make all"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release/sysinstall"	>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make all"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release"		>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make doRELEASE"			>> ${CHROOTDIR}/mk
	echo "RELEASEDIR=/R"			>> ${CHROOTDIR}/etc/make.conf
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr ; cvs co -P src
.else
	cd ${CHROOTDIR}/usr ; cvs export -r ${RELEASETAG} src
.endif
	( cd ${CHROOTDIR}/usr/src/sys/conf && \
	mv newvers.sh foo && \
	sed "s/^RELEASE=.*/RELEASE=${BUILDNAME}/" foo > newvers.sh )
	# To be used in development...
	#find ${.CURDIR} -print | cpio -dumpv ${CHROOTDIR}
	chmod 755 ${CHROOTDIR}/mk
	chroot ${CHROOTDIR} /mk

# Same as above, but assume that a build just fell over, has been corrected
# and now we just want to pick up where we left off.
rerelease:
.if !defined(CHROOTDIR) || !defined(BUILDNAME)
	@echo "To make a release you must set CHROOTDIR and BUILDNAME" && false
.endif
	@if [ ! -d ${CHROOTDIR} ]; then echo "No ${CHROOTDIR} directory to re-release in!"; exit 1; fi
	cd ${.CURDIR}/../etc ; ${MAKE} distrib-dirs DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/../etc ; ${MAKE} distribution DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/.. ; ${MAKE} -k install DESTDIR=${CHROOTDIR}
	echo "#!/bin/sh"			> ${CHROOTDIR}/mk
	echo "set -ex"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src"			>> ${CHROOTDIR}/mk
	echo "make all install"			>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release/libdisk"	>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make all"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release/sysinstall"	>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make all"				>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release"		>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make doRELEASE"			>> ${CHROOTDIR}/mk
	echo "RELEASEDIR=/R"			>> ${CHROOTDIR}/etc/make.conf
.if !defined(RELEASENOUPDATE)
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr ; cvs update -P -d -q src
.else
	cd ${CHROOTDIR}/usr ; cvs export -r ${RELEASETAG} -P -d -q src
.endif
.endif
	( cd ${CHROOTDIR}/usr/src/sys/conf && \
	mv newvers.sh foo && \
	sed "s/^RELEASE=.*/RELEASE=${BUILDNAME}/" foo > newvers.sh )
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
	mkdir ${RD}/floppies
	mkdir ${RD}/trees
	mkdir ${RD}/dists
	for i in ${ALL_DISTS} ; do \
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
	cd ${.CURDIR}/../etc && make distrib-dirs DESTDIR=${RD}/trees/bin
	cd ${.CURDIR}/.. ; make distribute DISTDIR=${RD}/trees
	chflags -R noschg ${RD}/trees
	uudecode ${.CURDIR}/compat20/libgcc.so.261.0.uu
	install -m 444 -o bin -g bin libgcc.so.261.0 ${RD}/trees/bin/usr/lib
	touch release.2

# Make and install a couple of kernels we need.
release.3:
	rm -rf ${RD}/kernels
	mkdir -p ${RD}/kernels
	@cd ${.CURDIR} && $(MAKE) ckRELEASEDIR
	cd ${.CURDIR}/../sys/i386/conf && \
		sed 's/GENERIC/BOOTCDROM/g' GENERIC > BOOTCDROM
	for i in GENERIC BOOTCDROM ; do \
		cd ${.CURDIR}/../sys/i386/conf && \
			config $$i && \
		cd ${.CURDIR}/../sys/compile/$$i && \
			make depend && \
			make kernel && \
			cp kernel ${RD}/kernels/$$i ; \
		rm -rf ${.CURDIR}/../sys/compile/$$i ; \
	done
	rm -f ${.CURDIR}/../sys/i386/conf/BOOTCDROM
	cd ${RD}/kernels && kzip GENERIC
	touch release.3

# Make and install the three crunched binaries which live on the floppies.
# You are not supposed to like this :-)
release.4:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	rm -rf ${RD}/crunch
	mkdir -p ${RD}/crunch
	for j in ${WHICH_CRUNCH} ; do \
		rm -rf $${j}_crunch && \
		mkdir $${j}_crunch && \
		( cd $${j}_crunch && \
		crunchgen ${.CURDIR}/$${j}_crunch.conf && \
		${MAKE} -f $${j}_crunch.mk objs exe NOCRYPT=yes \
		    "CFLAGS=${CFLAGS} -DCRUNCHED_BINARY") && \
		mv $${j}_crunch/$${j}_crunch ${RD}/crunch/$${j} && \
		rm -rf $${j}_crunch ; \
	done
	touch release.4

# Fix up the distributions.
release.5:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	ln -f ${RD}/kernels/GENERIC ${RD}/trees/bin/kernel.GENERIC
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
	rm -rf ${RD}/bootfd
	mkdir ${RD}/bootfd
	cd ${RD}/bootfd && \
		mkdir -p dev mnt stand/help
	ln -f ${RD}/kernels/GENERIC.kz ${RD}/bootfd/kernel
	@cd ${.CURDIR} ; $(MAKE) installCRUNCH CRUNCH=boot \
		DIR=${RD}/bootfd/stand ZIP=true
	( cd ${RD}/trees/bin/dev ; \
		ls console tty ttyv1 null zero \
		*[sw]d* cuaa[01] fd[01] rfd[01] \
		cd0a mcd0a scd0a matcd0a rst0 ft0 wt0 | \
	cpio -dump ${RD}/bootfd/dev )
	tar --exclude CVS -cf - -C ${.CURDIR}/sysinstall help | \
		tar xvf - -C ${RD}/bootfd/stand
	install -c ${.CURDIR}/../COPYRIGHT ${RD}/bootfd/stand/help/en_US.ISO8859-1/COPYRIGHT
	cd ${RD}/trees/bin/usr/share/FAQ/Text ; \
		install -c RELNOTES.FreeBSD ${RD}/bootfd/stand/help/en_US.ISO8859-1/RELNOTES; \
		install -c README ${RD}/bootfd/stand/help/en_US.ISO8859-1/README
	touch ${RD}/bootfd/this_is_boot_flp
	cd ${.CURDIR} ; ${MAKE} doFLOPPY FLOPPY=boot
	touch release.6

# Complete the cpiofd directory
release.7:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	rm -rf ${RD}/cpiofd
	mkdir ${RD}/cpiofd
	cd ${RD}/cpiofd && \
		mkdir stand stand/scripts
	ln -f ${RD}/kernels/GENERIC ${RD}/cpiofd/kernel
	@cd ${.CURDIR} ; $(MAKE) installCRUNCH CRUNCH=cpio \
		DIR=${RD}/cpiofd/stand ZIP=false
	cd ${RD}/trees/bin ; ls ${CPIO1} | cpio -dump ${RD}/cpiofd/stand
	tar --exclude CVS -cf - -C ${.CURDIR}/sysinstall info | tar xvf - -C ${RD}/cpiofd/stand
	cd ${RD}/cpiofd ; ( rm -f OK ; find . -print ; touch OK ; echo OK ) | cpio -H newc -oa | gzip -9 -c | dd conv=osync > ${RD}/floppies/cpio.flp
	touch release.7

# Make binary dists, notice that DES dist isn't finished yet.
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


# Make source dists, and finish of DES too
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
	if [ -f sdes.aa ] ; then mv sdes.* ../des ; fi && \
	if [ -f sebones.aa ] ; then mv sebones.* ../des ; fi && \
	sh -e ${.CURDIR}/scripts/mkchecksums.sh )
	cp ${.CURDIR}/scripts/extract_src.sh ${RD}/dists/src/extract.sh
	if [ -d ${RD}/dists/des ] ; then \
	( cd ${RD}/dists/des && sh -e ${.CURDIR}/scripts/mkchecksums.sh );\
	fi
	@echo "src distribution is finished."
	touch release.9

# Complete the fixitfd
release.10:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	rm -rf ${RD}/fixitfd
	mkdir ${RD}/fixitfd
	cd ${RD}/fixitfd && \
		mkdir stand bin sbin etc mnt mnt1 mnt2 mnt3 mnt4 tmp
	ln -f ${RD}/kernels/GENERIC.kz ${RD}/fixitfd/kernel
	@cd ${.CURDIR} ; $(MAKE) installCRUNCH CRUNCH=fixit \
		DIR=${RD}/fixitfd/stand ZIP=true
	( cd ${RD}/trees/bin/dev && \
		sh MAKEDEV wd0s5 wd0s6 wd0s7 wd0s8 && \
		ls console tty ttyv1 null zero \
		*[sw]d* fd[01] rfd[01] | \
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

	# This rule makes ${CHROOTDIR}/R/ftp a suitable anon ftp for testing.
ftp.2:
	-mkdir ${FD}/bin
	ln -f ${RD}/trees/bin/bin/ls ${FD}/bin/ls

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
	cd ${.CURDIR} && ${MAKE} cdrom.1 ftp.1
	@echo "Release done"

floppies:
	rm -f release.4 release.6 release.7
	cd ${.CURDIR} && ${MAKE} doRELEASE

boot.flp:
	rm -f release.4 release.6
	cd ${.CURDIR} && ${MAKE} release.4 release.6 WHICH_CRUNCH=boot

fixit.flp:
	rm -f release.4 release.10
	cd ${.CURDIR} && ${MAKE} release.4 release.10 WHICH_CRUNCH=fixit

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

installCRUNCH:
.if !defined(CRUNCH)
	@echo "CRUNCH undefined in installCRUNCH" ; exit 1
.endif
.if !defined(DIR)
	@echo "DIR undefined in installCRUNCH" ; exit 1
.endif
.if !defined(ZIP)
	@echo "ZIP undefined in installCRUNCH" ; exit 1
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
.include <bsd.prog.mk>
