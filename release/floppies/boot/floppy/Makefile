
#	$Id: Makefile,v 1.6 1997/07/16 12:24:23 julian Exp $
# If this is a RELEASE, then set

# Things which without too much trouble can be considered variables

# mountpoint for filesystems.
TOP=${.CURDIR}/../../../..
OBJTOP=${.OBJDIR}/../../../..
FS_BIN=../../bin
SCRIPTDIR=${.CURDIR}/../../bin

MNT=			/mnt

# other floppy parameters.
FSSIZE=			1200
FSLABEL=		fd1200
FSINODE=		4300

NEWFSARGS=		-c 80 -b 4096 -f 512 -i 4000 -m 0 -t 0 -u 0 \
				-o space -T ${FSLABEL}


# Upper size for the mfs in the boot.flp kernel.
# These are adjusted down to the minimum needed by doFS.sh.
BOOTMFSSIZE=		1200
MFSINODE=		40000

# Things which will get you into trouble if you change them
MTREEFILES=		${.CURDIR}/../etc/mtree


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

all: ${FS_BIN}/write_mfs_in_kernel
	cd ${.CURDIR} && ${MAKE} doMFSKERN 



#
# --==## Compile a kernel by name ${KERNEL} ##==--
#
# We don't erase the sys/compile/${KERNEL} directory, since somebody
# may want to reuse it (release.8 presently)
#
CONF=${TOP}/sys/i386/conf
COMPILE=${TOP}/sys/compile
doKERNEL:
	cd ${CONF} && config ${KERNEL}
	cd ${COMPILE}/${KERNEL} && \
			make  depend && \
			make  kernel && \

#
# --==## Put a filesystem into a BOOTMFS kernel ##==--
#
LABELDIR=${OBJTOP}/sys/i386/boot/biosboot

${COMPILE}/BOOTMFS/kernel:
	rm -f ${COMPILE}/BOOTMFS/mfs_vfsops.o
	cd ${CONF} && \
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
	echo "options \"MFS_ROOT=`cat ../mfs/fs-image.size`\"" >> \
		${CONF}/BOOTMFS
	cd ${.CURDIR} && ${MAKE} doKERNEL KERNEL=BOOTMFS

doMFSKERN: ${COMPILE}/BOOTMFS/kernel
	-mkdir tree
	cp ${COMPILE}/BOOTMFS/kernel tree
	${FS_BIN}/write_mfs_in_kernel/write_mfs_in_kernel tree/kernel ../mfs/fs-image
	kzip -v tree/kernel
	mv tree/kernel.kz tree/kernel
	cp ${COMPILE}/../i386/boot/biosboot/boot.help tree
	touch tree/boot.config
	@vnconfig /dev/vn0 ../mfs/fs-image
	@mkdir -p /tmp/mnt_xx
	@mount /dev/vn0 /tmp/mnt_xx 
	${FS_BIN}/dumpnlist/dumpnlist ${COMPILE}/BOOTMFS/kernel > /tmp/mnt_xx/stand/symbols
	@umount /tmp/mnt_xx
	@vnconfig -u /dev/vn0
	@rmdir /tmp/mnt_xx

	sh -e ${SCRIPTDIR}/doFS.sh ${LABELDIR} ${MNT} ${FSSIZE} tree \
		10000 ${FSLABEL}

clean:
	rm -rf tree fs-image fs-image.size

.include <bsd.prog.mk>

