#	$Id: Makefile,v 1.19 1994/11/06 11:24:58 jkh Exp $
#

#For a 1.2MB boot floppy.
#SMALL_FLOPPY=	yes

.if defined(SMALL_FLOPPY)
FDLABEL=	fd1200
DDCOUNT=	80
.else
FDLABEL=	fd1440
DDCOUNT=	96
.endif

FLOPPY=		fd0
DDBS=		15k
MNT=		/mnt

CPIO1= cat chmod cp date dd df echo ed expr hostname kill ln ls mkdir
CPIO1+= mt mv rcp rm rmdir sh sleep stty sync test [ -sh
CPIO1+= badsect chown clri disklabel dump dmesg fdisk fsck ifconfig init
CPIO1+= mknod mount mount_cd9660 mount_msdos mount_nfs ncftp newfs ping pwd
CPIO1+= reboot restore slattach swapon umount route
CPIO1+= rdump rrestore halt
CPIO1+= ftp rsh sed telnet rlogin 

CPIO2=  etc/services

CPIO3=	extract.sh
# Somewhat on the rough side...
CLEANFILES+=	*.o *.c *.cache *.mk *.lo ${CPIO1} *.flp *.gz 
CLEANFILES+=	boot_flp boot_${FDLABEL}.flp cpio_flp_1

MTREE_DIR=	${.CURDIR}/../etc/mtree

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

boot.flp: boot_${FDLABEL}.flp

boot_${FDLABEL}.flp:
	-umount ${MNT}
	-umount /dev/${FLOPPY}
	echo y | fdformat ${FLOPPY}
	disklabel -w -B \
		-b ${DESTDIR}/usr/mdec/fdboot -s ${DESTDIR}/usr/mdec/bootfd \
		/dev/r${FLOPPY} ${FDLABEL}
	newfs -c 80 -b 4096 -f 512 -i 4096 -m 0 -o space /dev/r${FLOPPY}
	mount -o async /dev/${FLOPPY} ${MNT}
	mkdir ${MNT}/dev ${MNT}/stand ${MNT}/mnt
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
	install -m 500 -c ${.CURDIR}/bininst ${MNT}/stand/bininst
.if !defined(SMALL_FLOPPY)
	install -m 400 -c ${.CURDIR}/../COPYRIGHT ${MNT}/COPYRIGHT
.endif
	install -m 400 -c ${.CURDIR}/../README ${MNT}/README
.if !defined(SMALL_FLOPPY)
	install -m 400 -c ${.CURDIR}/../share/FAQ/DISKSPACE.FAQ \
		${MNT}/DISKSPACE.FAQ
	install -m 400 -c ${.CURDIR}/../share/FAQ/RELNOTES.FreeBSD \
		${MNT}/RELNOTES.FreeBSD
.endif
	touch ${MNT}/this_is_boot_flp
	-umount ${MNT}
	fsck /dev/r${FLOPPY}
	dd if=/dev/r${FLOPPY} bs=${DDBS} count=${DDCOUNT} of=boot_${FDLABEL}.flp
	gzip -9 -v < boot_${FDLABEL}.flp > boot_${FDLABEL}.flp.gz
	ls -l boot.flp*

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

small.floppies:
	${MAKE} SMALL_FLOPPY=yes crunch boot.flp cpio.flp

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
	( cd ${.CURDIR}/../etc ; \
		${MAKE} bin-tarball DESTDIR=${RELEASEDIR}/filesys )
	( cd ${.CURDIR} ;  \
		${MAKE} floppies )

.include <bsd.prog.mk>
