#	$Id: Makefile,v 1.193.2.19 1995/10/04 20:28:43 jkh Exp $
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
# SET THIS !!!
BUILDNAME=2.1.0-951005-SNAP
CHROOTDIR=/a/release
# If this is a RELEASE, then set
RELEASETAG=RELENG_2_1_0

# Things which without too much trouble can be considered variables

EXPORT_DISTS=	games manpages proflibs dict info
EXTRA_DISTS=	krb des ${EXPORT_DISTS}
ALL_DISTS=	bin ${EXTRA_DISTS}

# Extra source tarballs; each argument is a pair of source dir and
# distribution name.  The dist name should not exceed 7 characters
# (another "s" for "source" will be prepended).
EXTRA_SRC+=	usr.sbin/sendmail/cf smailcf
#EXTRA_SRC+=	usr.sbin/config kconf

BOOT1=	etc/protocols etc/sysconfig

FAQS=	MIRROR.SITES

# define this to use a vn device instead of the floppy drive
VNDEVICE=		vn0

# mountpoint for filesystems.
MNT=			/mnt

# other floppy parameters.
FDSIZE=			1200
FDLABEL=		fd1200
FDINODE=		4300

NEWFSARGS=		-c 80 -b 4096 -f 512 -i 4000 -m 0 -t 0 -u 0 \
				-o space -T ${FDLABEL}

ZIPNSPLIT=		gzip --no-name -9 -c | split -b 240640 -

# Size of the mfs to put in the kernel we boot.
# You want to keep this as small as possible, it costs dearly in RAM.
BOOTMFSSIZE=		1075
MFSINODE=		70000

# Things which will get you into trouble if you change them
DISTRIBUTIONS=		bin ${EXTRA_DISTS}
MTREEFILES=		${.CURDIR}/../etc/mtree
RD=			${RELEASEDIR}/stage
FD=			${RELEASEDIR}/ftp
CD=			${RELEASEDIR}/cdrom

.if !defined(WHICH_CRUNCH)
WHICH_CRUNCH=	boot root fixit
.endif

release:
.if !defined(CHROOTDIR) || !defined(BUILDNAME)
	@echo "To make a release you must set CHROOTDIR and BUILDNAME" && false
.endif
.if exists(${CHROOTDIR})
	chflags -R noschg ${CHROOTDIR}/.
	rm -rf ${CHROOTDIR}
.endif
	mkdir -p ${CHROOTDIR}
	cd ${.CURDIR}/../etc ; ${MAKE} distrib-dirs DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/../etc ; ${MAKE} distribution DESTDIR=${CHROOTDIR}
	cd ${.CURDIR}/.. ; ${MAKE} install DESTDIR=${CHROOTDIR}
	echo "#!/bin/sh"			> ${CHROOTDIR}/mk
	echo "set -ex"				>> ${CHROOTDIR}/mk
	echo "CFLAGS='-O -pipe'"		>> ${CHROOTDIR}/mk
	echo "export CFLAGS"			>> ${CHROOTDIR}/mk
	echo "RELEASEDIR=/R"			>> ${CHROOTDIR}/mk
	echo "export RELEASEDIR"		>> ${CHROOTDIR}/mk
	echo "cd /usr/src"			>> ${CHROOTDIR}/mk
	echo "make world"			>> ${CHROOTDIR}/mk
	echo "cd /usr/src/release"		>> ${CHROOTDIR}/mk
	echo "make obj"				>> ${CHROOTDIR}/mk
	echo "make doRELEASE"			>> ${CHROOTDIR}/mk
	echo "echo Release Finished"		>> ${CHROOTDIR}/mk
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr ; cvs co -P src
.else
	cd ${CHROOTDIR}/usr ; cvs co -P -r ${RELEASETAG} src
.endif
	( cd ${CHROOTDIR}/usr/src/sys/conf && \
	  cp newvers.sh foo && \
	  sed "s/^RELEASE=.*/RELEASE=${BUILDNAME}/" foo > newvers.sh; rm foo )
	( cd ${CHROOTDIR}/usr/src/release/sysinstall && \
	  sed "s/__RELEASE/${BUILDNAME}/" version.h > version.h.new && mv version.h.new version.h )
	chmod 755 ${CHROOTDIR}/mk
	chroot ${CHROOTDIR} /mk

# Same as above but for re-rolling the release after simply changing a few
# things.
rerelease:
.if !defined(CHROOTDIR) || !defined(BUILDNAME)
	@echo "To make a re-release you must set CHROOTDIR and BUILDNAME" && false
.endif
	@if [ ! -d ${CHROOTDIR} ]; then echo "No ${CHROOTDIR} directory to re-release in!"; exit 1; fi
	echo "#!/bin/sh"			> ${CHROOTDIR}/mk
	echo "set -ex"				>> ${CHROOTDIR}/mk
	echo "CFLAGS='-O -pipe'"		>> ${CHROOTDIR}/mk
	echo "export CFLAGS"			>> ${CHROOTDIR}/mk
	echo "RELEASEDIR=/R"			>> ${CHROOTDIR}/mk
	echo "export RELEASEDIR"		>> ${CHROOTDIR}/mk
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
	echo "echo Re-Release Finished"		>> ${CHROOTDIR}/mk
.if !defined(RELEASENOUPDATE)
.if !defined(RELEASETAG)
	cd ${CHROOTDIR}/usr ; cvs update -P -d -q src
.else
	cd ${CHROOTDIR}/usr ; cvs update -P -d -q -r ${RELEASETAG} src
.endif
.endif
	( cd ${CHROOTDIR}/usr/src/sys/conf && \
	  mv newvers.sh foo && \
	  sed "s/^RELEASE=.*/RELEASE=${BUILDNAME}/" foo > newvers.sh; rm foo )
	( cd ${CHROOTDIR}/usr/src/release/sysinstall && \
	  sed "s/__RELEASE/${BUILDNAME}/" version.h > version.h.new && mv version.h.new version.h )
	chmod 755 ${CHROOTDIR}/mk
	chroot ${CHROOTDIR} /mk

whap:
	( cd ${CHROOTDIR}/usr/src/release/sysinstall && \
	  sed "s/__RELEASE/${BUILDNAME}/" version.h > version.h.new && mv version.h.new version.h )

clean:
	rm -rf root_crunch boot_crunch release.[0-9] release.1[0]

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
	cd ${.CURDIR}/../eBones && ( \
		make bootstrap ;\
		make obj all help-distribute DISTDIR=${RD}/trees ;\
		make kprog \
	)
	chflags -R noschg ${RD}/trees
	uudecode ${.CURDIR}/compat20/libgcc.so.261.0.uu
	install -m 444 -o bin -g bin libgcc.so.261.0 ${RD}/trees/bin/usr/lib
	touch release.2

# Make and install a couple of kernels we need.  To keep BOOTMFS smaller,
# we cull out certain options (see fgrep -v) before building it.
release.3:
	rm -rf ${RD}/kernels
	mkdir -p ${RD}/kernels
	@cd ${.CURDIR} && $(MAKE) ckRELEASEDIR
	cd ${.CURDIR}/../sys/i386/conf && \
		fgrep -v SYSV GENERIC | \
		fgrep -v pty | \
		fgrep -v PROCFS | \
		sed 's/GENERIC/BOOTMFS/g' > BOOTMFS && \
		echo "options  \"MFS_ROOT=${BOOTMFSSIZE}\"" >> BOOTMFS && \
		echo "options  MFS" >> BOOTMFS && \
		fgrep -v SYSV ATAPI | \
		fgrep -v pty | \
		fgrep -v PROCFS | \
		sed 's/ATAPI/BOOTMFS/g' > BOOTMFS_A && \
		echo "options  \"MFS_ROOT=${BOOTMFSSIZE}\"" >> BOOTMFS_A && \
		echo "options  MFS" >> BOOTMFS_A

	for i in BOOTMFS BOOTMFS_A GENERIC ATAPI ; do \
		cd ${.CURDIR}/../sys/i386/conf && \
			config $$i && \
		cd ${.CURDIR}/../sys/compile/$$i && \
			make depend && \
			make kernel && \
			cp kernel ${RD}/kernels/$$i ; \
		rm -rf ${.CURDIR}/../sys/compile/$$i ; \
	done
	rm -f ${.CURDIR}/../sys/i386/conf/BOOTCDROM
	touch release.3

# Make and install the three crunched binaries which live on the floppies.
# You are not supposed to like this :-)
release.4:
	cd ${.CURDIR}/libdisk && make obj && make depend && make all
	cd ${.CURDIR}/sysinstall && make obj && make depend && make all
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
	ln -f ${RD}/kernels/ATAPI ${RD}/trees/bin/kernel.ATAPI
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

# Make binary dists, notice that DES dist isn't finished yet.
release.6:
	@cd ${.CURDIR} ; $(MAKE) ckRELEASEDIR
	rm -rf ${RD}/dists
	mkdir -p ${RD}/dists
	mkdir -p ${RD}/info
	@for i in ${DISTRIBUTIONS} ; \
	do \
		if [ -d ${RD}/trees/$${i} ] ; then \
			cd ${.CURDIR} ; \
				$(MAKE) doTARBALL SD=${RD}/trees/$${i} \
					TN=$$i TD=$$i ARG="." ; \
			echo "$${i} distribution is finished."; \
		fi ; \
	done
	touch release.6


# Make source dists, and finish off DES too
release.7:
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
.if defined(EXTRA_SRC)
	@set ${EXTRA_SRC} ; \
	while [ $$# -ge 2 ] ; do \
		if [ -d /usr/src/$$1 -a "$$1" != "CVS" ] ; then \
			cd ${.CURDIR} ; \
			$(MAKE) doTARBALL SD=/usr/src \
				TN="s$$2" \
				TD=src ARG="$$1" ; \
		fi ; \
		shift ; shift ; \
	done
.endif
	( cd ${RD}/dists/src && \
	if [ -f ssecure.aa ] ; then mv ssecure.* ../des ; fi && \
	if [ -f sebones.aa ] ; then mv sebones.* ../des ; fi )
	@echo "src distribution is finished."
	mv ${RD}/dists/krb/* ${RD}/dists/des
	rmdir ${RD}/dists/krb
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
	@cd ${.CURDIR} && $(MAKE) ckRELEASEDIR
	rm -rf ${RD}/mfsfd
	mkdir ${RD}/mfsfd
	cd ${RD}/mfsfd && \
		mkdir -p dev mnt stand/help
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=boot \
		DIR=${RD}/mfsfd/stand ZIP=false
	( cd ${RD}/trees/bin/dev && \
		ls console tty ttyv0 ttyv1 ttyv2 ttyv3 null zero \
		*[sw]d* cuaa[01] fd[01] rfd[01] \
		cd0a mcd0a scd0a matcd0a wcd0a rst0 ft0 rwt0 | \
	cpio -dump ${RD}/mfsfd/dev )
	( cd ${RD}/mfsfd/dev && rm -f *[sw]d*[bdefgh] )
	cd ${RD}/trees/bin && ls ${BOOT1} | cpio -dump ${RD}/mfsfd/stand
	echo "nameserver      42/tcp name"	> ${RD}/mfsfd/stand/etc/services
	echo "ftp             21/tcp"		>> ${RD}/mfsfd/stand/etc/services
	echo "domain          53/tcp          nameserver" >> ${RD}/mfsfd/stand/etc/services
	echo "domain          53/udp          nameserver" >> ${RD}/mfsfd/stand/etc/services
	tar --exclude CVS -cf - -C ${.CURDIR}/sysinstall help | \
		tar xvf - -C ${RD}/mfsfd/stand
	install -c ${.CURDIR}/../COPYRIGHT ${RD}/mfsfd/stand/help/COPYRIGHT.hlp
	( 								\
	a=`expr ${BOOTMFSSIZE} \* 2` &&					\
	echo &&								\
	echo "mfs${BOOTMFSSIZE}:ty=mfs:se#512:nt#1:rm#300:\\" &&	\
	echo "	:ns#$$a:nc#1:\\" &&					\
	echo "	:pa#$$a:oa#0:ba#4096:fa#512:\\" &&			\
	echo "	:pc#$$a:oc#0:bc#4096:fc#512:" &&			\
	echo   								\
	) >> /etc/disktab
	cd ${.CURDIR} && ${MAKE} doFS FSSIZE=${BOOTMFSSIZE} \
		FSPROTO=${RD}/mfsfd FSLABEL=mfs${BOOTMFSSIZE} \
		FSINODE=${MFSINODE}

	# first the standard boot floppy, with GENERIC kernel
	rm -rf ${RD}/bootfd
	mkdir ${RD}/bootfd
	cp ${RD}/kernels/BOOTMFS ${RD}/bootfd/kernel
	./write_mfs_in_kernel ${RD}/bootfd/kernel fs-image
	kzip ${RD}/bootfd/kernel
	mv ${RD}/bootfd/kernel ${RD}/kernels/MFSKERNEL
	mv ${RD}/bootfd/kernel.kz ${RD}/bootfd/kernel
	ls -l ${RD}/bootfd/kernel
	cd ${.CURDIR} && ${MAKE} doFLOPPY FLOPPY=boot

	# Now the IDE CDROM floppy, with ATAPI kernel
	rm -rf ${RD}/atapifd
	mkdir ${RD}/atapifd
	cp ${RD}/kernels/BOOTMFS_A ${RD}/atapifd/kernel
	./write_mfs_in_kernel ${RD}/atapifd/kernel fs-image
	kzip ${RD}/atapifd/kernel
	mv ${RD}/atapifd/kernel ${RD}/kernels/MFSKERNEL_A
	mv ${RD}/atapifd/kernel.kz ${RD}/atapifd/kernel
	ls -l ${RD}/atapifd/kernel
	cd ${.CURDIR} && ${MAKE} doFLOPPY FLOPPY=atapi
	touch release.8

# Complete the rootfd directory
release.9:
	@cd ${.CURDIR} && $(MAKE) ckRELEASEDIR
	rm -rf ${RD}/rootfd
	mkdir ${RD}/rootfd
	cd ${RD}/rootfd && \
		mkdir stand stand/info
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=root \
		DIR=${RD}/rootfd/stand ZIP=false
	tar --exclude CVS -cf - -C ${RD} info | tar xvf - -C ${RD}/rootfd/stand
	mv ${RD}/rootfd/stand/info/krb/* ${RD}/rootfd/stand/info/des
	rmdir ${RD}/rootfd/stand/info/krb
	mv ${RD}/rootfd/stand/info/src/ssecure.inf ${RD}/rootfd/stand/info/des
	mv ${RD}/rootfd/stand/info/src/sebones.inf ${RD}/rootfd/stand/info/des
	tar -cf - -C ${RD}/trees/bin/dev MAKEDEV | tar xvf - -C ${RD}/rootfd/stand
	cd ${RD}/rootfd && ( rm -f OK && find . -print && touch OK && echo OK ) | cpio -H newc -oa | gzip -9 -c | dd conv=osync > ${RD}/floppies/root.tmp
	mv ${RD}/floppies/root.tmp ${RD}/floppies/root.flp
	touch release.9

# Complete the fixitfd
release.10:
	@cd ${.CURDIR} && $(MAKE) ckRELEASEDIR
	rm -rf ${RD}/fixitfd
	mkdir ${RD}/fixitfd
	cd ${RD}/fixitfd && \
		mkdir stand bin sbin etc mnt mnt1 mnt2 mnt3 mnt4 tmp
	@cd ${.CURDIR} && $(MAKE) installCRUNCH CRUNCH=fixit \
		DIR=${RD}/fixitfd/stand ZIP=true
	( cd ${RD}/trees/bin/dev && \
		sh MAKEDEV all && \
		find . -print | \
	cpio -dump ${RD}/fixitfd/dev )
	cp ${RD}/trees/bin/etc/spwd.db ${RD}/fixitfd/etc
	cp ${.CURDIR}/fixit.profile ${RD}/fixitfd/.profile
	cd ${.CURDIR} && ${MAKE} doFLOPPY FLOPPY=fixit
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
	cd ${RD} && find floppies dists -print | cpio -dumpl ${CD}
	ln -f ${RD}/kernels/MFSKERNEL ${CD}/kernel
	ln -f ${RD}/kernels/MFSKERNEL_A ${CD}/kernel.atapi
	ln -f ${RD}/trees/bin/usr/mdec/fbsdboot.exe ${CD}
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
		${ZIPNSPLIT} ${RD}/dists/${TD}/$$tn. && \
		cd ${RD}/info && \
		mkdir -p ${TD} && \
		chmod a+x ${.CURDIR}/info.sh && \
		${.CURDIR}/info.sh ${RD}/dists/${TD}/$$tn > ${TD}/$$tn.inf \
	)

doRELEASE:  release.1 release.2 release.3 release.4 release.5 release.6 \
		release.7 release.8 release.9 release.10
	cd ${.CURDIR} && ${MAKE} cdrom.1 ftp.1
	@echo "Release done"

floppies:
	rm -f release.4 release.8 release.9
	cd ${.CURDIR} && ${MAKE} doRELEASE

boot.flp:
	rm -f release.4 release.8
	cd ${.CURDIR} && ${MAKE} release.4 release.8 WHICH_CRUNCH=boot

root.flp:
	rm -f release.4 release.9
	cd ${.CURDIR} && ${MAKE} release.4 release.9 WHICH_CRUNCH=root

fixit.flp:
	rm -f release.4 release.10
	cd ${.CURDIR} && ${MAKE} release.4 release.10 WHICH_CRUNCH=fixit

write_mfs_in_kernel:	${.CURDIR}/write_mfs_in_kernel.c
	${CC} ${CFLAGS} -o write_mfs_in_kernel ${.CURDIR}/write_mfs_in_kernel.c

ckRELEASEDIR:
.if !defined(RELEASEDIR)
	@echo "To make a release RELEASEDIR must be defined" && false
.endif

doFLOPPY:
.if !defined(FLOPPY)
	@echo "FLOPPY undefined in doFLOPPY" ; exit 1
.endif
	rm -f ${RD}/floppies/${FLOPPY}.flp
	cd ${.CURDIR} && ${MAKE} doFS FSSIZE=${FDSIZE} \
		FSPROTO=${RD}/${FLOPPY}fd \
		FSLABEL=${FDLABEL} FSINODE=${FDINODE}
	mv fs-image ${RD}/floppies/${FLOPPY}.flp

doFS:
.if !defined(FSSIZE)
	@echo "FSSIZE undefined in doFS" ; exit 1
.endif
.if !defined(FSPROTO)
	@echo "FSPROTO undefined in doFS" ; exit 1
.endif
.if !defined(FSINODE)
	@echo "FSINODE undefined in doFS" ; exit 1
.endif
	rm -f fs-image
	if [ ! -b /dev/${VNDEVICE} -o ! -c /dev/r${VNDEVICE} ] ; then \
		cd /dev && sh MAKEDEV ${VNDEVICE} ; fi
	@umount /dev/${VNDEVICE} 2>/dev/null || true
	@umount ${MNT} 2>/dev/null || true
	@vnconfig -u /dev/r${VNDEVICE} 2>/dev/null || true
	dd of=fs-image if=/dev/zero count=${FSSIZE} bs=1k 2>/dev/null
	vnconfig -c /dev/r${VNDEVICE} fs-image
.if defined(FSLABEL)
	disklabel -w -r -B \
		-b ${RD}/trees/bin/usr/mdec/fdboot \
		-s ${RD}/trees/bin/usr/mdec/bootfd \
		/dev/r${VNDEVICE} ${FSLABEL}
.endif
	newfs -u 0 -t 0 -i ${FSINODE} -m 0 -T ${FSLABEL} /dev/r${VNDEVICE}
	mount /dev/${VNDEVICE} ${MNT}
	cd ${FSPROTO} && find . -print | cpio -dump ${MNT}
	df -i /mnt
	umount ${MNT}
	fsck  /dev/r${VNDEVICE} < /dev/null

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
