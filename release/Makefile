#	$Id: Makefile,v 1.72 1995/01/12 12:30:05 jkh Exp $
#

FLOPPY=		fd0


# Evil floppies are, of course, 1.2MB floppies.
.if defined(EVIL_FLOPPY)
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
CPIO3=	miscfuncs.sh instdist.sh netinst.sh adduser.sh bininst

CPIO4=	/usr/sbin/tzsetup
CPIO4B=	tzsetup

# Somewhat on the rough side...
CLEANFILES+=	*.o *.c *.cache *.mk *.lo ${CPIO1} *.flp *.gz 
CLEANFILES+=	boot_flp cpio_flp_1

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
	if [ ! -f ${.CURDIR}/../sys/compile/GENERIC ]; then \
		(cd ${.CURDIR}/../sys/i386/conf; config GENERIC); \
		(cd ${.CURDIR}/../sys/compile/GENERIC; \
			${MAKE} depend; ${MAKE} all; ) \
	fi

boot_12.flp: 
	(cd ${.CURDIR}; EVIL_FLOPPY=yes ${MAKE} boot.flp)
	mv boot.flp boot_12.flp
	gzip -9 -c < boot_12.flp > boot_12.flp.gz &

boot_144.flp:
	(cd ${.CURDIR}; ${MAKE} boot.flp)
	mv boot.flp boot_144.flp
	gzip -9 -c < boot_144.flp > boot_144.flp.gz &

boot.flp: 
	crunchgen ${.CURDIR}/boot_flp.conf
	${MAKE} -f boot_flp.mk objs exe NOCRYPT=yes
	-umount ${MNT}
	-umount /dev/${FLOPPY}
	# echo y | fdformat ${FLOPPY}
	disklabel -w -B \
		-b ${DESTDIR}/usr/mdec/fdboot -s ${DESTDIR}/usr/mdec/bootfd \
		/dev/r${FLOPPY} ${FDLABEL}
	newfs -c 80 -b 4096 -f 512 -i 10000 -m 0 -o space /dev/r${FLOPPY}
	mount -o async /dev/${FLOPPY} ${MNT}
	mkdir ${MNT}/dev ${MNT}/stand ${MNT}/mnt
	strip -x ${.CURDIR}/../sys/compile/GENERIC/kernel
	cp ${.CURDIR}/../sys/compile/GENERIC/kernel ${MNT}/kernel
	cp ${DESTDIR}/usr/mdec/sdboot ${DESTDIR}/usr/mdec/bootsd ${MNT}/stand
	( cd ${DESTDIR}/dev ; \
		ls console tty ttyv1 null zero \
		sd[0123][a-h] wd[0123][a-h] fd[01] \
		rsd[0123][a-h] rwd[0123][a-h] rfd[01] \
	| cpio -dumpv ${MNT}/dev \
	)
	gzip -9 < boot_flp > ${MNT}/stand/sysinstall
	chmod 755 ${MNT}/stand/sysinstall
	ln ${MNT}/stand/sysinstall ${MNT}/stand/newfs
	ln ${MNT}/stand/sysinstall ${MNT}/stand/gzip
	ln ${MNT}/stand/sysinstall ${MNT}/stand/fsck
.if !defined(EVIL_FLOPPY)
	install -m 400 -c ${.CURDIR}/../COPYRIGHT ${MNT}/COPYRIGHT
	install -m 400 -c ${.CURDIR}/../share/FAQ/diskspace.FAQ \
		${MNT}/DISKSPACE.FAQ
	install -m 400 -c ${.CURDIR}/../share/FAQ/RELNOTES.FreeBSD \
		${MNT}/RELNOTES.FreeBSD
.endif
	install -m 400 -c ${.CURDIR}/../share/FAQ/TROUBLESHOOTING \
		${MNT}/TROUBLESHOOTING
	install -m 400 -c ${.CURDIR}/../share/FAQ/README-2.0 ${MNT}/README
	touch ${MNT}/this_is_boot_flp
	-umount ${MNT}
	fsck /dev/r${FLOPPY}
	dd if=/dev/r${FLOPPY} bs=${DDBS} count=${DDCOUNT} of=boot.flp
	df -k /dev/r${FLOPPY}

cpio.flp:
	crunchgen ${.CURDIR}/cpio_flp_1.conf
	${MAKE} -f cpio_flp_1.mk objs exe NOCRYPT=yes
	for i in ${CPIO1} ; do rm -f ./$$i ; ln cpio_flp_1 ./$$i ; done
	( cd /${DESTDIR} ; ls ${CPIO2} | cpio -H newc -oa ) | cpio -ivd
	( cd ${.CURDIR} ; ls ${CPIO3} | cpio -H newc -oa ) | cpio -ivd
	cp ${CPIO4} .
	ls ${CPIO1} ${CPIO2} ${CPIO3} ${CPIO4B} | \
		cpio -H newc -oa | gzip -9 | \
		dd conv=osync > cpio.flp
	gzip -9 -v < cpio.flp > cpio.flp.gz &

floppies: boot_144.flp boot_12.flp cpio.flp

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
	cp ${.CURDIR}/extract.sh ${RELEASEDIR}/tarballs/bindist
	( cd ${RELEASEDIR}/tarballs/bindist; ${.CURDIR}/mkchecksums.sh )
	( cd ${.CURDIR} ;  \
		${MAKE} floppies )
foo:
	( cd ${.CURDIR}/../etc ; \
		${MAKE} srcebones-tarball \
			DESTDIR=${RELEASEDIR}/filesys \
			NOCRYPT=yes SHARED=copies )
		
bar:
	( cd ${.CURDIR}/../etc ; \
		${MAKE} des-tarball \
			DESTDIR=${RELEASEDIR}/filesys \
			SHARED=copies )

EXTRADISTRIBUTIONS=	secrdist srcdist games manpages proflibs dict infodist
DISTRIBUTIONS=	bindist ${EXTRADISTRIBUTIONS}
MTREEFILES=	${.CURDIR}/../etc/mtree
SECRSRC=	src/secure src/eBones

distribute: 
	-mkdir ${RELEASEDIR}
	chflags -R noschg ${RELEASEDIR}/.
	rm -rf ${RELEASEDIR}/*
	for i in ${DISTRIBUTIONS} ; \
	do \
	mkdir ${RELEASEDIR}/$$i ; \
	mtree -deU -f ${MTREEFILES}/BSD.root.dist -p ${RELEASEDIR}/$$i/ ; \
	mtree -deU -f ${MTREEFILES}/BSD.var.dist -p ${RELEASEDIR}/$$i/var ; \
	mtree -deU -f ${MTREEFILES}/BSD.usr.dist -p ${RELEASEDIR}/$$i/usr ; \
	done 
	( cd ${.CURDIR} ;  \
		${MAKE} kernel DESTDIR=${RELEASEDIR}/filesys )
	install ${COPY} -m 644 ${.CURDIR}/../sys/compile/GENERIC/kernel \
		${RELEASEDIR}/bindist/kernel

beforetarballs:
	rm -rf ${RELEASEDIR}/manpages/usr/share/man
	mv ${RELEASEDIR}/bindist/usr/share/man \
		${RELEASEDIR}/manpages/usr/share/man
	rm -rf ${RELEASEDIR}/games/usr/games
	mv ${RELEASEDIR}/bindist/usr/games \
		${RELEASEDIR}/games/usr/games
	rm -rf ${RELEASEDIR}/games/usr/share/games
	mv ${RELEASEDIR}/bindist/usr/share/games \
		${RELEASEDIR}/games/usr/share/games
	rm -rf ${RELEASEDIR}/dict/usr/share/dict
	mv ${RELEASEDIR}/bindist/usr/share/dict \
		${RELEASEDIR}/dict/usr/share/dict
	mv ${RELEASEDIR}/bindist/usr/share/misc/airport \
	   ${RELEASEDIR}/bindist/usr/share/misc/birthtoken \
	   ${RELEASEDIR}/bindist/usr/share/misc/flowers \
	   ${RELEASEDIR}/bindist/usr/share/misc/na.phone \
	   ${RELEASEDIR}/bindist/usr/share/misc/zipcodes \
		${RELEASEDIR}/dict/usr/share/misc
	mv ${RELEASEDIR}/bindist/usr/lib/*_p.a \
		${RELEASEDIR}/proflibs/usr/lib
	-cd ${RELEASEDIR} ; \
	find ${EXTRADISTRIBUTIONS} -depth -type d -print | xargs rmdir
	mkdir -p ${RELEASEDIR}/secrdist/usr
	cd ${RELEASEDIR}/secrdist/usr ; cvs export -r ${RELEASETAG} ${SECRSRC}
	mkdir -p ${RELEASEDIR}/srcdist/usr
	cd ${RELEASEDIR}/srcdist/usr ; cvs export -r ${RELEASETAG} src
	cd ${RELEASEDIR}/srcdist/usr/src ; rm -rf secure kerberosIV eBones

tarballs:
	rm -rf ${RELEASEDIR}/tarballs
	mkdir ${RELEASEDIR}/tarballs
	@for i in ${DISTRIBUTIONS} ; \
	do \
		mkdir ${RELEASEDIR}/tarballs/$${i} ; \
		( cd ${RELEASEDIR}/$${i}; \
			tar cf - . | \
			${ZIPNSPLIT} ${RELEASEDIR}/tarballs/$$i/$${i}.) ; \
	( cd ${RELEASEDIR}/tarballs/$${i}; sh -e ${.CURDIR}/mkchecksums.sh ) ; \
	cp ${.CURDIR}/extract.sh ${RELEASEDIR}/tarballs/$${i} ;\
	echo "$${i} distribution is finished."; \
	done

.include <bsd.prog.mk>
