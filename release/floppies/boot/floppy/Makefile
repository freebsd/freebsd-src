
#	$Id: Makefile,v 1.214 1996/05/02 08:18:28 jkh Exp $
# If this is a RELEASE, then set

# Things which without too much trouble can be considered variables

# mountpoint for filesystems.
TOP=${.CURDIR}/../../../..
FS_BIN=${.CURDIR}/../..

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
doKERNEL:
	cd ${TOP}/sys/i386/conf && config -n ${KERNEL}
	cd ${TOP}/sys/compile/${KERNEL} && \
			make  depend && \
			make  kernel && \

#
# --==## Put a filesystem into a BOOTMFS kernel ##==--
#
LABELDIR=${TOP}/sys/i386/boot/biosboot/obj
CONF=${TOP}/sys/i386/conf
COMPILE=${TOP}/sys/compile
doMFSKERN:
	rm -f ${TOP}/sys/compile/BOOTMFS/mfs_vfsops.o
	cd ${CONF} && \
		fgrep -v SYSV GENERIC | \
		fgrep -v pty | \
		fgrep -v PROCFS | \
		sed 's/GENERIC/BOOTMFS/g' | \
		sed '/maxusers/s/10/4/' > BOOTMFS && \
		echo "options  MFS" >> BOOTMFS && \
		echo "options  NFS_NOSERVER" >> BOOTMFS && \
		echo 'options  "MAXCONS=4"' >> BOOTMFS 
	echo "options \"MFS_ROOT=`cat ../mfs/fs-image.size`\"" >> \
		${CONF}/BOOTMFS
	cd ${.CURDIR} && ${MAKE} doKERNEL KERNEL=BOOTMFS
	-mkdir tree
	cp ${COMPILE}/BOOTMFS/kernel tree
	${FS_BIN}/write_mfs_in_kernel tree/kernel ../mfs/fs-image
	kzip tree/kernel
	mv tree/kernel.kz tree/kernel
	ls -l tree/kernel
	sh -e ${FS_BIN}/doFS.sh ${LABELDIR} ${MNT} ${FSSIZE} tree \
		10000 ${FSLABEL}

clean:
	rm -rf tree fs-image fs-image.size

.include <bsd.prog.mk>
