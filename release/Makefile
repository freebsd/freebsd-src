#	$Id: Makefile,v 1.6 1994/10/28 06:42:38 jkh Exp $
#

FLOPPY=		fd0
FDLABEL=	fd1200
DDBS=		15k
DDCOUNT=	80
MNT=		/mnt

CPIO1= cat chmod cp date dd df echo ed expr hostname kill ln ls mkdir
CPIO1+= mt mv pwd rcp rm rmdir sh sleep stty sync test [ -sh
CPIO1+= badsect chown clri disklabel dump dmesg fdisk fsck ifconfig init
CPIO1+= mknod mount newfs ping reboot restore swapon umount route
CPIO1+= rdump rrestore halt
CPIO1+= ftp rsh sed telnet rlogin 

CPIO2=  etc/services

# Somewhat on the rough side...
CLEANFILES=	*.o *.c *.cache *.mk *.lo ${CPIO1} *.flp *.gz

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

boot.flp:
	-umount ${MNT}
	-umount /dev/${FLOPPY}
	echo y | fdformat ${FLOPPY}
	disklabel -w -B \
		-b ${DESTDIR}/usr/mdec/fdboot -s ${DESTDIR}/usr/mdec/bootfd \
		/dev/r${FLOPPY} ${FDLABEL}
	newfs -c 80 -b 4096 -f 512 /dev/r${FLOPPY}
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
	install -c -m 400 ${.CURDIR}/../COPYRIGHT ${MNT}/COPYRIGHT
	install -c -m 400 ${.CURDIR}/../README ${MNT}/README
	touch ${MNT}/this_is_boot_flp
	-umount ${MNT}
	fsck /dev/r${FLOPPY}
	dd if=/dev/r${FLOPPY} bs=${DDBS} count=${DDCOUNT} of=boot.flp
	gzip -9 -v < boot.flp > boot.flp.gz
	ls -l boot.flp*

cpio.flp:
	( cd /${DESTDIR} ; ls ${CPIO2} | cpio -H newc -oa ) | cpio -ivd
	for i in ${CPIO1} ; do rm -f ./$$i ; ln cpio_flp_1 ./$$i ; done
	ls ${CPIO1} ${CPIO2} | cpio -H newc -oa | gzip -9 > cpio.flp
	gzip -9 -v < cpio.flp > cpio.flp.gz
	ls -l cpio.flp*

crunch:
	crunchgen ${.CURDIR}/boot_flp.conf
	${MAKE} -f boot_flp.mk objs exe
	crunchgen ${.CURDIR}/cpio_flp_1.conf
	${MAKE} -f cpio_flp_1.mk objs exe

floppies: crunch boot.flp cpio.flp

release20: 
	-mkdir ${RELEASEDIR}
	chflags -R noschg ${RELEASEDIR}/.
	rm -rf ${RELEASEDIR}/*
	( cd ${.CURDIR}/.. ;  \
		${MAKE} world NOCRYPT=yes)
	( cd ${.CURDIR}/../etc ;  \
		${MAKE} release-dirs )
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
