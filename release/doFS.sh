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

	sed '/^minimum:/,$d' /etc/disktab > /etc/disktab.tmp
	cat /etc/disktab.tmp > /etc/disktab
	rm -f /etc/disktab.tmp
	(
	a=`expr ${FSSIZE} \* 2`
	echo 
	echo "minimum:ty=mfs:se#512:nt#1:rm#300:\\"
	echo "	:ns#$a:nc#1:\\"
	echo "	:pa#$a:oa#0:ba#4096:fa#512:\\"
	echo "	:pc#$a:oc#0:bc#4096:fc#512:"
	echo
	) >> /etc/disktab

	disklabel -w -r -B \
		-b ${RD}/trees/bin/usr/mdec/fdboot \
		-s ${RD}/trees/bin/usr/mdec/bootfd \
		/dev/r${VNDEVICE} minimum

	newfs -u 0 -t 0 -i ${FSINODE} -m 0 -T minimum -o space /dev/r${VNDEVICE}a

	mount /dev/${VNDEVICE}a ${MNT}

	( set -e && cd ${FSPROTO} && find . -print | cpio -dump ${MNT} )

	set `df -i /mnt | tail -1`

	umount ${MNT}

	fsck -p /dev/r${VNDEVICE}a < /dev/null

	vnconfig -u /dev/r${VNDEVICE} 2>/dev/null || true

	if [ $FSLABEL != "minimum" ] ; then
		echo ${FSSIZE} > fs-image.size
		break
	fi

	echo ">>> Filesystem is ${FSSIZE} K, $4 left"
	echo ">>>     ${FSINODE} bytes/inode, $7 left"
	echo ">>>   `expr ${FSSIZE} \* 1024 / ${FSINODE}`"

# As far as I can tell, the following has only really caused me great
# difficulty..
#

#	if [ $4 -gt 128 ] ; then
#		echo "Reducing size"
#		FSSIZE=`expr ${FSSIZE} - $4 / 2`
#		continue
#	fi
#	if [ $7 -gt 128 ] ; then
#		echo "Increasing bytes per inode"
#		FSINODE=`expr ${FSINODE} + 8192`
#		continue
#	fi
#	if [ $4 -gt 32 ] ; then
#		echo "Reducing size"
#		FSSIZE=`expr ${FSSIZE} - 4`
#		FSINODE=`expr ${FSINODE} - 1024`
#		continue
#	fi
#	if [ $7 -gt 64 ] ; then
#		echo "Increasing bytes per inode"
#		FSINODE=`expr ${FSINODE} + 8192`
#		continue
#	fi
#	if [ $deadlock -eq 0 ] ; then
#		echo "Avoiding deadlock, giving up"
#		echo ${FSSIZE} > fs-image.size
#		break
#	fi
#	deadlock=`expr $deadlock - 1`
	echo ${FSSIZE} > fs-image.size
	break;
done
