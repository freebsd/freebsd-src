#	$Id: Makefile,v 1.35 1994/11/11 22:27:11 jkh Exp $
#

# Evil floppies are, of course, 1.2MB floppies.
FLOPPY=		fd0
EVIL_FLOPPY=	yes

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

CPIO1= basename cat chmod cp date dd df echo ed expr hostname kill ln ls mkdir
CPIO1+= mt mv rcp rm rmdir sh sleep stty sync test tip [ -sh
CPIO1+= badsect chown clri disklabel dump dmesg fdisk fsck ifconfig init
CPIO1+= mknod mount mount_cd9660 mount_msdos mount_nfs ncftp newfs ping pwd
CPIO1+= reboot restore slattach swapon umount route
CPIO1+= rdump rrestore halt ft
CPIO1+= ftp rsh sed telnet rlogin grep

CPIO2=  etc/services etc/protocols

CPIO3=	bininst
# Somewhat on the rough side...
CLEANFILES+=	*.o *.c *.cache *.mk *.lo ${CPIO1} *.flp *.gz 
CLEANFILES+=	boot_flp boot.flp cpio_flp_1

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
	(cd ${.CURDIR}/../sys/i386/conf; config GENERIC)
	(cd ${.CURDIR}/../sys/compile/GENERIC; ${MAKE} depend; ${MAKE} all; )

boot.flp: 
	-umount ${MNT}
	-umount /dev/${FLOPPY}
	# echo y | fdformat ${FLOPPY}
	disklabel -w -B \
		-b ${DESTDIR}/usr/mdec/fdboot -s ${DESTDIR}/usr/mdec/bootfd \
		/dev/r${FLOPPY} ${FDLABEL}
	newfs -c 80 -b 4096 -f 512 -i 8192 -m 0 -o space /dev/r${FLOPPY}
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
	install -m 400 -c ${.CURDIR}/../COPYRIGHT ${MNT}/COPYRIGHT
	install -m 400 -c ${.CURDIR}/../share/FAQ/README-2.0 ${MNT}/README
	install -m 400 -c ${.CURDIR}/../share/FAQ/DISKSPACE.FAQ \
		${MNT}/DISKSPACE.FAQ
	install -m 400 -c ${.CURDIR}/../share/FAQ/RELNOTES.FreeBSD \
		${MNT}/RELNOTES.FreeBSD
	touch ${MNT}/this_is_boot_flp
	-umount ${MNT}
	fsck /dev/r${FLOPPY}
	dd if=/dev/r${FLOPPY} bs=${DDBS} count=${DDCOUNT} of=boot.flp
	gzip -9 -v < boot.flp > boot.flp.gz
	df -k /dev/r${FLOPPY}

cpio.flp:
	for i in ${CPIO1} ; do rm -f ./$$i ; ln cpio_flp_1 ./$$i ; done
	( cd /${DESTDIR} ; ls ${CPIO2} | cpio -H newc -oa ) | cpio -ivd
	( cd ${.CURDIR} ; ls ${CPIO3} | cpio -H newc -oa ) | cpio -ivd
	ls ${CPIO1} ${CPIO2} ${CPIO3} | cpio -H newc -oa | gzip -9 > cpio.flp
	gzip -9 -v < cpio.flp > cpio.flp.gz
	ls -l cpio.flp*

crunch:
	crunchgen ${.CURDIR}/boot_flp.conf
	${MAKE} -f boot_flp.mk objs exe
	crunchgen ${.CURDIR}/cpio_flp_1.conf
	${MAKE} -f cpio_flp_1.mk objs exe

floppies: crunch boot.flp cpio.flp

release20: 
	( cd ${.CURDIR} ; ${MAKE} clean)
	-mkdir ${RELEASEDIR}
	chflags -R noschg ${RELEASEDIR}/.
	rm -rf ${RELEASEDIR}/*
	( cd ${.CURDIR}/.. ;  \
		${MAKE} world NOCRYPT=yes)
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
	(cd ${RELEASEDIR/filesys}; \
		tar cf - . | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/bindist/bin_tgz.)
	cp ${.CURDIR}/extract.sh ${RELEASEDIR}/tarballs/bindist
	( cd ${.CURDIR} ;  \
		${MAKE} floppies )

.include <bsd.prog.mk>
