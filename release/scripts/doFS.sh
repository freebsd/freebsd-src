:
#set -ex

if [ "x$VNDEVICE" = "x" ] ; then
	VNDEVICE=vn0
fi
export BLOCKSIZE=512

if [ "$1" = "-s" ]; then
	do_size="yes"; shift
else
	do_size=""
fi

FSIMG=$1; shift
RD=$1 ; shift
MNT=$1 ; shift
FSSIZE=$1 ; shift
FSPROTO=$1 ; shift
FSINODE=$1 ; shift
FSLABEL=$1 ; shift

deadlock=20

u=`expr $VNDEVICE : 'vn\([0-9]*\)' || true`
rm -f /dev/*vnn*
mknod /dev/vnn${u} c 43 `expr 65538 + $u '*' 8`
mknod /dev/rvnn${u} c 43 `expr 65538 + $u '*' 8`
mknod /dev/vnn${u}c c 43 `expr 2 + $u '*' 8`
mknod /dev/rvnn${u}c c 43 `expr 2 + $u '*' 8`
VNDEVICE=vnn$u

while true 
do
	rm -f ${FSIMG}

	umount /dev/${VNDEVICE} 2>/dev/null || true

	umount ${MNT} 2>/dev/null || true

	vnconfig -u /dev/r${VNDEVICE} 2>/dev/null || true

	dd of=${FSIMG} if=/dev/zero count=${FSSIZE} bs=1k 2>/dev/null
	# this suppresses the `invalid primary partition table: no magic'
	awk 'BEGIN {printf "%c%c", 85, 170}' |\
	    dd of=${FSIMG} obs=1 seek=510 conv=notrunc 2>/dev/null

	vnconfig -s labels -c /dev/r${VNDEVICE} ${FSIMG}
	disklabel -Brw /dev/r${VNDEVICE} ${FSLABEL}
	newfs -i ${FSINODE} -T ${FSLABEL} -o space /dev/r${VNDEVICE}c
	tunefs -m 0 /dev/r${VNDEVICE}c

	mount /dev/${VNDEVICE}c ${MNT}

	if [ -d ${FSPROTO} ]; then
		(set -e && cd ${FSPROTO} && find . -print | cpio -dump ${MNT})
	else
		cp -p ${FSPROTO} ${MNT}
	fi

	df -ki ${MNT}

	set `df -ki ${MNT} | tail -1`

	umount ${MNT}
	vnconfig -u /dev/r${VNDEVICE} 2>/dev/null || true

	echo ">>> Filesystem is ${FSSIZE} K, $4 left"
	echo ">>>     ${FSINODE} bytes/inode, $7 left"
	if [ "${do_size}" ]; then
		echo ${FSSIZE} > ${FSIMG}.size
	fi
	break;
done
rm -f /dev/*vnn*
