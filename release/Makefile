#	$Id: Makefile,v 1.96 1995/01/30 03:22:24 phk Exp $
#

FLOPPY=		fd0
#VNDEV=		vn0


# Evil floppies are, of course, 1.44MB floppies.
.if !defined(EVIL_FLOPPY)
FDLABEL=	fd1200
DDBS=		15k
DDCOUNT=	80
.else
FDLABEL=	fd1440
DDBS=		18k
DDCOUNT=	80
.endif

MNT=		/mnt

CPIO1= basename cat chmod cksum cp cu date dd df dmesg echo ed expr hostname 
CPIO1+= kill ln ls mkdir mt mv rcp rm rmdir sh sleep stty sync tar test tip [ 
CPIO1+= -sh badsect chown clri disklabel dump dmesg fdisk fsck getopt ifconfig 
CPIO1+= init mknod mount mount_cd9660 mount_msdos mount_nfs ncftp newfs ping pwd
CPIO1+= reboot restore slattach swapon umount route
CPIO1+= rdump rrestore halt ft
CPIO1+= ftp rsh sed telnet rlogin grep

CPIO2=  etc/services etc/protocols

# bininst MUST be the last file on the cpio floppy.  It's used to detect
# a successful extraction.
CPIO3=	scripts/miscfuncs.sh scripts/instdist.sh scripts/netinst.sh \
	scripts/adduser.sh scripts/bininst.sh

CPIO4=	/usr/sbin/tzsetup
CPIO4B=	tzsetup

# Somewhat on the rough side...
CLEANFILES+=	*.o *.c *.cache *.mk *.lo ${CPIO1} *.flp *.gz 
CLEANFILES+=	boot_flp cpio_flp_1 kernel.*

MTREE_DIR=	${.CURDIR}/../etc/mtree

ZIPNSPLIT=	gzip --no-name -9 -c | split -b 240640 -

hierarchy:
	mtree -deU -f ${MTREE_DIR}/BSD.root.dist  -p ${DESTDIR}/
	mtree -deU -f ${MTREE_DIR}/BSD.var.dist   -p ${DESTDIR}/var
	mtree -deU -f ${MTREE_DIR}/BSD.usr.dist   -p ${DESTDIR}/usr
.if defined(MAKE_LOCAL)
	mtree -deU -f ${MTREE_DIR}/BSD.local.dist -p ${DESTDIR}/usr/local
.endif
	rm -f ${DESTDIR}/sys
	ln -s usr/src/sys ${DESTDIR}/sys

kernel:	${.CURDIR}/../sys/i386/conf/GENERIC
	if [ ! -f ${.CURDIR}/../sys/compile/GENERIC/kernel ]; then \
		(cd ${.CURDIR}/../sys/i386/conf; config GENERIC); \
		(cd ${.CURDIR}/../sys/compile/GENERIC; \
			${MAKE} depend; ${MAKE} all; ) \
	fi
	cp ${.CURDIR}/../sys/compile/GENERIC/kernel kernel

kernel.BOOTFLP:	${.CURDIR}/../sys/i386/conf/BOOTFLP
	if [ ! -f ${.CURDIR}/../sys/compile/BOOTFLP/kernel ]; then \
		(cd ${.CURDIR}/../sys/i386/conf; config BOOTFLP); \
		(cd ${.CURDIR}/../sys/compile/BOOTFLP; \
			${MAKE} depend; ${MAKE} all; ) \
	fi
	cp ${.CURDIR}/../sys/compile/BOOTFLP/kernel kernel.BOOTFLP
	strip kernel.BOOTFLP

boot.flp: kernel.BOOTFLP
	crunchgen ${.CURDIR}/boot_flp.conf
	${MAKE} -f boot_flp.mk objs exe NOCRYPT=yes
	-umount /dev/${FLOPPY}
	-umount ${MNT}
	disklabel -w -r -B \
		-b ${DESTDIR}/usr/mdec/fdboot -s ${DESTDIR}/usr/mdec/bootfd \
		/dev/r${FLOPPY} ${FDLABEL}
	newfs -c 80 -b 4096 -f 512 -i 9000 -m 0 -o space -T ${FDLABEL} \
		/dev/r${FLOPPY}
	mount -o async /dev/${FLOPPY} ${MNT}
	mkdir ${MNT}/dev ${MNT}/stand ${MNT}/mnt
	cp kernel.BOOTFLP ${MNT}/kernel
	( cd ${DESTDIR}/dev ; \
		ls console tty ttyv1 null zero \
		sd[0123][a-h] wd[0123][a-h] fd[01] \
		rsd[0123][a-h] rwd[0123][a-h] rfd[01] \
	| cpio -dump ${MNT}/dev \
	)
	gzip -9 -c < boot_flp > ${MNT}/stand/sysinstall
	chmod 755 ${MNT}/stand/sysinstall
	ln ${MNT}/stand/sysinstall ${MNT}/stand/newfs
	ln ${MNT}/stand/sysinstall ${MNT}/stand/gzip
	ln ${MNT}/stand/sysinstall ${MNT}/stand/fsck
	install -m 400 -c ${.CURDIR}/../COPYRIGHT ${MNT}/COPYRIGHT
	install -m 400 -c ${.CURDIR}/../share/FAQ/diskspace.FAQ \
		${MNT}/DISKSPACE.FAQ
	install -m 400 -c ${.CURDIR}/../share/FAQ/RELNOTES.FreeBSD \
		${MNT}/RELNOTES.FreeBSD
	install -m 400 -c ${.CURDIR}/../share/FAQ/TROUBLESHOOTING \
		${MNT}/TROUBLESHOOTING
	install -m 400 -c ${.CURDIR}/../share/FAQ/README-2.0 ${MNT}/README
	touch ${MNT}/this_is_boot_flp
	sync
	-umount ${MNT}
	sync
	fsck /dev/r${FLOPPY}
	dd if=/dev/r${FLOPPY} of=boot.tmp bs=${DDBS} count=${DDCOUNT}
	mv boot.tmp boot.flp
	od -x boot.flp | head -3

cpio.flp: kernel
	crunchgen ${.CURDIR}/cpio_flp_1.conf
	${MAKE} -f cpio_flp_1.mk objs exe NOCRYPT=yes
	for i in ${CPIO1} ; do rm -f ./$$i ; ln cpio_flp_1 ./$$i ; done
	( cd /${DESTDIR} ; ls ${CPIO2} | cpio -H newc -oa ) | cpio -ivd
	( cd ${.CURDIR} ; ls ${CPIO3} | cpio -H newc -oa ) | cpio -ivd
	chmod 755 ${CPIO3}
	cp ${CPIO4} .
	rm -f OK
	touch OK
	(ls ${CPIO1} ${CPIO2} ${CPIO3} ${CPIO4B} ; \
		echo kernel ; echo OK ) | \
		cpio -H newc -oa | gzip -9 -c | \
		dd conv=osync > cpio.flp
	rm -f OK
	gzip -1 -c < cpio.flp > cpio.flp.gz

floppies: boot.flp cpio.flp
	mkdir -p ${RELEASEDIR}/floppies
	cp boot.flp boot.flp.gz cpio.flp cpio.flp.gz \
		${RELEASEDIR}/floppies

release20: 
.if !defined(RELEASEDIR)
	@echo "Do NOT invoke this target without a RELEASEDIR defined."
	@echo "It will otherwise blow away your root directory!"
	@exit 1
.endif
	@echo "Did you remember to increment the release number?"
	( cd ${.CURDIR} ; ${MAKE} clean)
	-mkdir ${RELEASEDIR}
	chflags -R noschg ${RELEASEDIR}/.
	rm -rf ${RELEASEDIR}/*
	mkdir -p ${RELEASEDIR}/tarballs
	( cd ${.CURDIR}/.. ;  \
		${MAKE} all distribute NOCRYPT=yes)
	( cd ${.CURDIR}/../etc ;  \
		${MAKE} release-dirs )
	( cd ${.CURDIR} ; ${MAKE} obj)
	( cd ${.CURDIR} ;  \
		${MAKE} kernel DESTDIR=${RELEASEDIR}/filesys )
	install ${COPY} -m 644 ${.CURDIR}/../sys/compile/GENERIC/kernel \
		${RELEASEDIR}/filesys/kernel
	( cd ${.CURDIR} ;  \
		${MAKE} hierarchy DESTDIR=${RELEASEDIR}/filesys )
	( cd ${.CURDIR}/../etc ; \
		${MAKE} distribution DESTDIR=${RELEASEDIR}/filesys \
			NOCRYPT=yes SHARED=copies)
	( cd ${RELEASEDIR}/filesys; \
		tar cf - . | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/bindist/bindist.)
	cp ${.CURDIR}/scripts/extract_bin.sh \
		${RELEASEDIR}/tarballs/bindist/extract.sh
	( cd ${RELEASEDIR}/tarballs/bindist; ${.CURDIR}/scripts/mkchecksums.sh )
	( cd ${.CURDIR} ;  \
		${MAKE} floppies )

EXTRADISTRIBUTIONS=	secure games manpages proflibs dict info
DISTRIBUTIONS=	bin ${EXTRADISTRIBUTIONS}
MTREEFILES=	${.CURDIR}/../etc/mtree

jkhdist:
	@echo "--> Making all"
	( cd ${.CURDIR}/..; cvs update -P -d -q; ${MAKE} all )
	@echo "--> Making distribute"
	( cd ${.CURDIR}/..; ${MAKE} distribute )
	@echo "--> Making beforetarballs"
	( cd ${.CURDIR}; ${MAKE} beforetarballs )
	@echo "--> Making tarballs"
	( cd ${.CURDIR}; ${MAKE} tarballs )
	@echo "--> Making floppies"
	( cd ${.CURDIR}; ${MAKE} floppies )

distribute: 
	-mkdir ${RELEASEDIR}
	chflags -R noschg ${RELEASEDIR}/.
	rm -rf ${RELEASEDIR}/*
	for i in ${DISTRIBUTIONS} ; \
	do \
	mkdir -p ${RELEASEDIR}/$$i ; \
	mtree -deU -f ${MTREEFILES}/BSD.root.dist -p ${RELEASEDIR}/$$i/ ; \
	mtree -deU -f ${MTREEFILES}/BSD.var.dist -p ${RELEASEDIR}/$$i/var ; \
	mtree -deU -f ${MTREEFILES}/BSD.usr.dist -p ${RELEASEDIR}/$$i/usr ; \
	done 
	( cd ${.CURDIR} ;  \
		${MAKE} kernel DESTDIR=${RELEASEDIR}/filesys )
	install ${COPY} -m 644 ${.CURDIR}/../sys/compile/GENERIC/kernel \
		${RELEASEDIR}/bin/kernel

beforetarballs:
	rm -rf ${RELEASEDIR}/manpages/usr/share/man
	mv ${RELEASEDIR}/bin/usr/share/man \
		${RELEASEDIR}/manpages/usr/share/man
	rm -rf ${RELEASEDIR}/games/usr/games
	mv ${RELEASEDIR}/bin/usr/games \
		${RELEASEDIR}/games/usr/games
	rm -rf ${RELEASEDIR}/games/usr/share/games
	mv ${RELEASEDIR}/bin/usr/share/games \
		${RELEASEDIR}/games/usr/share/games
	rm -rf ${RELEASEDIR}/dict/usr/share/dict
	mv ${RELEASEDIR}/bin/usr/share/dict \
		${RELEASEDIR}/dict/usr/share/dict
	mv ${RELEASEDIR}/bin/usr/share/misc/airport \
	   ${RELEASEDIR}/bin/usr/share/misc/birthtoken \
	   ${RELEASEDIR}/bin/usr/share/misc/flowers \
	   ${RELEASEDIR}/bin/usr/share/misc/na.phone \
	   ${RELEASEDIR}/bin/usr/share/misc/zipcodes \
		${RELEASEDIR}/dict/usr/share/misc
	mv ${RELEASEDIR}/bin/usr/lib/*_p.a \
		${RELEASEDIR}/proflibs/usr/lib
	-cd ${RELEASEDIR} ; \
	find ${EXTRADISTRIBUTIONS} -depth -type d -print | xargs rmdir
	mkdir -p ${RELEASEDIR}/secure/usr

	mkdir -p ${RELEASEDIR}/src/usr
	cd ${RELEASEDIR}/src/usr ; cvs export -r ${RELEASETAG} src

tarballs:
	rm -rf ${RELEASEDIR}/tarballs
	mkdir -p ${RELEASEDIR}/tarballs
	@for i in ${DISTRIBUTIONS} ; \
	do \
		mkdir -p ${RELEASEDIR}/tarballs/$${i} ; \
		( cd ${RELEASEDIR}/$${i}; \
			tar cf - . | \
			${ZIPNSPLIT} ${RELEASEDIR}/tarballs/$$i/$${i}.) ; \
	( cd ${RELEASEDIR}/tarballs/$${i}; \
		sh -e ${.CURDIR}/scripts/mkchecksums.sh ) ; \
	cp ${.CURDIR}/scripts/extract_$${i}.sh \
		${RELEASEDIR}/tarballs/$${i}/extract.sh;\
	echo "$${i} distribution is finished."; \
	done

srcbase-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - [A-Z]* | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/base.)

srcbin-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - bin | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/srcbin.)

srcetc-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - etc | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/etc.)

srcgames-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - games | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/games.)

srcgnu-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - gnu | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/gnu.)

srcinclude-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - include | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/include.)

srclib-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - lib | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/lib.)

srclibexec-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - libexec | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/libexec.)

srcrelease-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - release | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/release.)

srcsbin-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - sbin | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/sbin.)

srcsecure-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/secure; \
		tar -cf - secure | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/secure/secrsrc.)

srclkm-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - lkm | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/lkm.)

srcerelease-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - release | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/release.)

srcebones-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/secure; \
		tar -cf - eBones | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/secure/ebones.)

srcshare-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - share | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/share.)

srcsys-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - sys lkm | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/sys.)

srcusrbin-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - usr.bin | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/usrbin.)

srcusrsbin-tarball:
	(cd ${RELEASEDIR}/src/usr/src; \
		mkdir -p ${RELEASEDIR}/tarballs/src; \
		tar -cf - usr.sbin | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/src/usrsbin.)

src:	srcbase-tarball srcbin-tarball srcetc-tarball \
	srcgames-tarball srcgnu-tarball srcinclude-tarball \
	srclib-tarball srcsecure-tarball srclibexec-tarball \
	srcsbin-tarball srcshare-tarball srcsys-tarball \
	srcusrbin-tarball srcusrsbin-tarball srcrelease-tarball \
	srcebones-tarball srclkm-tarball srcrelease-tarball
	( cd ${RELEASEDIR}/tarballs/src; \
		sh -e ${.CURDIR}/scripts/mkchecksums.sh )
	cp ${.CURDIR}/scripts/extract_src.sh \
		${RELEASEDIR}/tarballs/src/extract.sh

.include <bsd.prog.mk>
