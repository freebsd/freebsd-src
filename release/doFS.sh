:
#set -ex

VNDEVICE=vn0
export BLOCKSIZE=512

RD=$1 ; shift
MNT=$1 ; shift
FSSIZE=$1 ; shift
FSPROTO=$1 ; shift
FSINODE=$1 ; shift
FSLABEL=$1 ; shift

deadlock=20

while true 
do
	rm -f fs-image

	if [ ! -b /dev/${VNDEVICE} -o ! -c /dev/r${VNDEVICE} ] ; then 
		( cd /dev && sh MAKEDEV ${VNDEVICE} )
	fi

	umount /dev/${VNDEVICE} 2>/dev/null || true

	umount ${MNT} 2>/dev/null || true

	vnconfig -u /dev/r${VNDEVICE} 2>/dev/null || true

	dd of=fs-image if=/dev/zero count=${FSSIZE} bs=1k 2>/dev/null
	# this suppresses the `invalid primary partition table: no magic'
	awk 'BEGIN {printf "%c%c", 85, 170}' |\
	    dd of=fs-image obs=1 seek=510 conv=notrunc 2>/dev/null

	vnconfig -s labels -c /dev/r${VNDEVICE} fs-image

	dd if=${RD}/trees/bin/usr/mdec/boot1 of=fs-image conv=notrunc
	disklabel /dev/r${VNDEVICE} | disklabel -R -B \
		-b ${RD}/trees/bin/usr/mdec/fdboot \
		-s ${RD}/trees/bin/usr/mdec/bootfd \
		/dev/r${VNDEVICE} /dev/stdin

	newfs -u 0 -t 0 -i ${FSINODE} -m 0 -T minimum -o space /dev/r${VNDEVICE}c

	mount /dev/${VNDEVICE}c ${MNT}

	( set -e && cd ${FSPROTO} && find . -print | cpio -dump ${MNT} )

	set `df -i /mnt | tail -1`

	umount ${MNT}

	fsck -p /dev/r${VNDEVICE}c < /dev/null

	vnconfig -u /dev/r${VNDEVICE} 2>/dev/null || true

	if [ $FSLABEL != "minimum" ] ; then
		echo ${FSSIZE} > fs-image.size
		break
	fi

	echo ">>> Filesystem is ${FSSIZE} K, $4 left"
	echo ">>>     ${FSINODE} bytes/inode, $7 left"
	echo ">>>   `expr ${FSSIZE} \* 1024 / ${FSINODE}`"
	echo ${FSSIZE} > fs-image.size
	break;
done
