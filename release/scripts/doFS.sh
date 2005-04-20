#!/bin/sh
#
# $FreeBSD$
#

set -ex

export BLOCKSIZE=512

DISKLABEL=$1; shift
MACHINE=${1:+"-m $1"}; shift
FSIMG=$1; shift
RD=$1 ; shift
MNT=$1 ; shift
FSSIZE=$1 ; shift
FSPROTO=$1 ; shift
FSINODE=$1 ; shift
FSLABEL=$1 ; shift

#
# If we've been told to, compute the required file system size
# and average inode size automatically.
#
if [ ${FSSIZE} -eq 0 -a ${FSLABEL} = "auto" ]; then
	roundup() echo $((($1+$2-1)-($1+$2-1)%$2))
	nf=$(find ${FSPROTO} |wc -l)
	sk=$(du -sk ${FSPROTO} |cut -f1)
	FSINODE=$(roundup $(($sk*1024/$nf)) ${FSINODE})
	FSSIZE=$(roundup $(($sk*12/10)) 1024)
fi

dofs_vn () {
	if [ "x$VNDEVICE" = "x" ] ; then
		VNDEVICE=vn0
	fi
	u=`expr $VNDEVICE : 'vn\([0-9]*\)' || true`
	VNDEVICE=vnn$u

	rm -f /dev/*vnn*
	mknod /dev/rvnn${u} c 43 `expr 65538 + $u '*' 8`
	mknod /dev/rvnn${u}c c 43 `expr 2 + $u '*' 8`
	mknod /dev/vnn${u} b 15 `expr 65538 + $u '*' 8`
	mknod /dev/vnn${u}c b 15 `expr 2 + $u '*' 8`

	umount /dev/${VNDEVICE} 2>/dev/null || true
	umount ${MNT} 2>/dev/null || true
	vnconfig -u /dev/r${VNDEVICE} 2>/dev/null || true

	vnconfig -s labels -c /dev/r${VNDEVICE} ${FSIMG}

	trap "umount ${MNT}; vnconfig -u /dev/r${VNDEVICE}; rm -f /dev/*vnn*" EXIT

	disklabel -w ${BOOT} ${VNDEVICE} ${FSLABEL}
	newfs -i ${FSINODE} -o space -m 0 /dev/r${VNDEVICE}c

	mount /dev/${VNDEVICE}c ${MNT}
}

dofs_md () {
	if [ "x${MDDEVICE}" != "x" ] ; then
		umount /dev/${MDDEVICE} 2>/dev/null || true
		umount ${MNT} 2>/dev/null || true
		mdconfig -d -u ${MDDEVICE} 2>/dev/null || true
	fi

	MDDEVICE=`mdconfig -a -t vnode -f ${FSIMG}`
	if [ ! -c /dev/${MDDEVICE} ] ; then
		echo "No /dev/$MDDEVICE" 1>&2
		exit 1
	fi

	trap "umount ${MNT}; mdconfig -d -u ${MDDEVICE}" EXIT

	${DISKLABEL} ${MACHINE} -w ${BOOT} ${MDDEVICE} ${FSLABEL}
	newfs -O1 -i ${FSINODE} -o space -m 0 /dev/${MDDEVICE}c

	mount /dev/${MDDEVICE}c ${MNT}
}

rm -f ${FSIMG}
dd of=${FSIMG} if=/dev/zero count=${FSSIZE} bs=1k 2>/dev/null

#
# We don't have any bootblocks on ia64. Note that -B implies -r,
# so we have to specifically specify -r when we don't have -B.
# bsdlabel fails otherwise.
#
case `uname -r` in
4.*)
	if [ -f "${RD}/trees/base/boot/boot1" ]; then
		BOOT="-B -b ${RD}/trees/base/boot/boot1"
		if [ -f "${RD}/trees/base/boot/boot2" ]; then
			BOOT="${BOOT} -s ${RD}/trees/base/boot/boot2"
		fi
	else
		BOOT="-r"
	fi
	dofs_vn
	;;
*)
	if [ -f "${RD}/trees/base/boot/boot" ]; then
		BOOT="-B -b ${RD}/trees/base/boot/boot"
	else
		BOOT="-r"
	fi
	dofs_md
	;;
esac

if [ -d ${FSPROTO} ]; then
	(set -e && cd ${FSPROTO} && find . -print | cpio -dump ${MNT})
else
	cp -p ${FSPROTO} ${MNT}
fi

df -ki ${MNT}

set `df -ki ${MNT} | tail -1`

echo "*** File system is ${FSSIZE} K, $4 left"
echo "***     ${FSINODE} bytes/inode, $7 left"
