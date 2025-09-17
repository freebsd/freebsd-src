#!/bin/sh
#
# Copyright (c) 2005 Poul-Henning Kamp All rights reserved.
# Copyright (c) 2016 M. Warner Losh <imp@FreeBSD.org>
# Copyright (c) 2025 Karl Denninger <karl@denninger.net>
#
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

# Media geometry, only relevant if bios doesn't understand LBA.
[ -n "$NANO_SECTS" ] || NANO_SECTS=63
[ -n "$NANO_HEADS" ] || NANO_HEADS=16

#
# If there is no EFI partition size specified make it 40m
#
[ -n "$NANO_EFISIZE" ] || NANO_EFISIZE=$((40*1024*1000/512))


# Functions and variable definitions used by the new nanobsd
# image building system.  Align partitions to 1M
#

calculate_partitioning ( ) (
# Add the data and config sizes together for the last slice;
# we then use this in the next calculation
#
	echo $NANO_CONFSIZE $NANO_DATASIZE |
	awk '
	{
		ds = ($1 + $2)
		print ds
	}
	' > ${NANO_LOG}/_.data_size

	DATA_SIZE=`head -n 1 ${NANO_LOG}/_.data_size`
#
# Now compute the code partition size after leaving room for the data and
# EFI partitions, specifically rounding to the integer full megabyte.
# We prevent gpart from screwing with us on alignment (it likes to fit
# DOWNWARD!) by doing an explicit resize after we create the two code
# partitions so the created image will fit exactly.
#
	echo $NANO_MEDIASIZE $NANO_IMAGES $DATA_SIZE $NANO_EFISIZE |
	awk '
	{
		if ($2 > 1) {
			cs = int(((($1 - $3) / 2) - $4) / 1048576) * 1048576
		} else {
			cs = int((($1 - $3) - $4) / 1048576) * 1048576
		}
		print cs
	}
	' > ${NANO_LOG}/_.code_size
)

create_code_slice ( ) (
	pprint 2 "build code slice"
	pprint 3 "log: ${NANO_OBJ}/_.cs"

	(
	IMG=${NANO_DISKIMGDIR}/_.disk.image
	MNT=${NANO_OBJ}/_.mnt
	mkdir -p ${MNT}
	CODE_SIZE=`head -n 1 ${NANO_LOG}/_.code_size`

	if [ "${NANO_MD_BACKING}" = "swap" ] ; then
		MD=`mdconfig -a -t swap -s ${CODE_SIZE} -x ${NANO_SECTS} \
			-y ${NANO_HEADS}`
	else
		echo "Creating md backing file..."
		rm -f ${IMG}
		dd if=/dev/zero of=${IMG} seek=${CODE_SIZE} count=0
		MD=`mdconfig -a -t vnode -f ${IMG} -x ${NANO_SECTS} \
			-y ${NANO_HEADS}`
	fi

	trap "echo 'Running exit trap code' ; df -i ${MNT} ; umount ${MNT} || true ; mdconfig -d -u $MD" 1 2 15 EXIT

	gpart create -s bsd "${MD}"
	gpart add -t freebsd-ufs -b 16 "${MD}"
	if [ -f ${NANO_WORLDDIR}/boot/boot ]; then
	    echo "Making bootable partition"
	    gpart bootcode -b ${NANO_WORLDDIR}/boot/boot ${MD}
	else
	    echo "Partition will not be bootable"
	fi
	gpart list ${MD}

	# Create first image
	populate_slice /dev/${MD}${NANO_PARTITION_ROOT} ${NANO_WORLDDIR} ${MNT} "${NANO_ROOT}"
	mount /dev/${MD}a ${MNT}
	echo "Generating mtree..."
	( cd ${MNT} && mtree -c ) > ${NANO_OBJ}/_.mtree
	( cd ${MNT} && du -k ) > ${NANO_OBJ}/_.du
	nano_umount ${MNT}

	if [ "${NANO_MD_BACKING}" = "swap" ] ; then
		echo "Writing out _.disk.image..."
		dd conv=sparse if=/dev/${MD} of=${NANO_DISKIMGDIR}/_.disk.image bs=64k
	fi
	mdconfig -d -u $MD

	trap - 1 2 15 EXIT

	) > ${NANO_OBJ}/_.cs 2>&1
)


create_diskimage ( ) (
	pprint 2 "build diskimage"
	pprint 3 "log: ${NANO_OBJ}/_.di"

	(

	IMG=${NANO_DISKIMGDIR}/${NANO_IMGNAME}
	MNT=${NANO_OBJ}/_.mnt
	mkdir -p ${MNT}

	if [ "${NANO_MD_BACKING}" = "swap" ] ; then
		MD=`mdconfig -a -t swap -s ${NANO_MEDIASIZE} -x ${NANO_SECTS} \
			-y ${NANO_HEADS}`
	else
		echo "Creating md backing file..."
		rm -f ${IMG}
		dd if=/dev/zero of=${IMG} seek=${NANO_MEDIASIZE} count=0
		MD=`mdconfig -a -t vnode -f ${IMG} -x ${NANO_SECTS} \
			-y ${NANO_HEADS}`
	fi

	CODE_SIZE=`head -n 1 ${NANO_LOG}/_.code_size`
	DATA_SIZE=`head -n 1 ${NANO_LOG}/_.data_size`
	gpart create -s mbr ${MD}
	gpart bootcode -b /boot/mbr ${MD}
	gpart add -t freebsd -a 1m -i 1 -s ${CODE_SIZE} ${MD}
	gpart resize -i 1 -s ${CODE_SIZE} ${MD}
	if [ "${NANO_IMAGES}" = 2 ] ; then
		gpart add -a 1m -t freebsd -i 2 -s ${CODE_SIZE} ${MD}
		gpart resize -i 2 -s ${CODE_SIZE} ${MD}
	fi
	gpart add -a 1m -t efi -i 3 -s ${NANO_EFISIZE} ${MD}
#
# Use the rest for the last slice including conf and (if asked for) data
#
	gpart add -a 1m -t freebsd -i 4 ${MD}
	gpart create -s bsd ${MD}s4
	echo add ${NANO_CONFSIZE} requested
	gpart add -a 1m -t freebsd-ufs -i 1 -s ${NANO_CONFSIZE} ${MD}s4
	if [ ${NANO_DATASIZE} != 0 ];
	then
		echo add ${NANO_DATASIZE} requested
		gpart add -a 1m -t freebsd-ufs -i 4 -s ${NANO_DATASIZE} ${MD}s4
	fi

	gpart set -a active -i 1 ${MD}

	trap "echo 'Running exit trap code' ; df -i ${MNT} ; nano_umount ${MNT} || true ; mdconfig -d -u $MD" 1 2 15 EXIT

	gpart show ${MD}
#
# If there is an EFI loader (there should be) stick it in the image
#
	if [ -f ${NANO_WORLDDIR}/boot/loader.efi ]; then
		echo "Copying in the EFI loader...."
		newfs_msdos -F 32 -c 1 ${MD}s3
		mount -t msdosfs /dev/${MD}s3 ${MNT}
		mkdir -p ${MNT}/EFI/BOOT
		mkdir -p ${MNT}/EFI/FreeBSD
		cp ${NANO_WORLDDIR}/boot/loader.efi ${MNT}/EFI/BOOT/${NANO_EFI_BOOTNAME}
#
# Optionally we could put the rootdev in EFI/FreeBSD but.... there
# is a potential problem with this if the EFI firmware finds something else
# with a filesystem on it "before" the stick -- so don't.
#
#		echo "rootdev=disk0s1a" >${MNT}/EFI/FreeBSD/loader.env
#
		umount ${MNT}
	fi

	echo "Writing code image..."
	dd conv=sparse if=${NANO_DISKIMGDIR}/_.disk.image of=/dev/${MD}${NANO_SLICE_ROOT} bs=64k

	if [ $NANO_IMAGES -gt 1 -a $NANO_INIT_IMG2 -gt 0 ] ; then
		# Duplicate to second image (if present)
		echo "Duplicating to second image..."
		dd conv=sparse if=/dev/${MD}${NANO_SLICE_ROOT} of=/dev/${MD}${NANO_SLICE_ALTROOT} bs=64k
		mount /dev/${MD}${NANO_ALTROOT} ${MNT}
		for f in ${MNT}/etc/fstab ${MNT}/conf/base/etc/fstab
		do
			sed -i "" "s=${NANO_DRIVE}${NANO_SLICE_ROOT}=${NANO_DRIVE}${NANO_SLICE_ALTROOT}=g" $f
		done
		nano_umount ${MNT}
		# Override the label from the first partition so we
		# don't confuse glabel with duplicates.
		if [ -n "${NANO_LABEL}" ]; then
			tunefs -L ${NANO_LABEL}"${NANO_ALTROOT}" /dev/${MD}${NANO_ALTROOT}
		fi
	fi

	# Create Config slice
	populate_cfg_slice /dev/${MD}${NANO_SLICE_CFG} "${NANO_CFGDIR}" ${MNT} "${NANO_SLICE_CFG}"

	# Create Data slice, if any.
	if [ -n "$NANO_SLICE_DATA" -a "$NANO_SLICE_CFG" = "$NANO_SLICE_DATA" -a \
	   "$NANO_DATASIZE" -ne 0 ]; then
		pprint 2 "NANO_SLICE_DATA is the same as NANO_SLICE_CFG, fix."
		exit 2
	fi
	if [ $NANO_DATASIZE -ne 0 -a -n "$NANO_SLICE_DATA" ] ; then
		populate_data_slice /dev/${MD}${NANO_SLICE_DATA} "${NANO_DATADIR}" ${MNT} "${NANO_SLICE_DATA}"
	fi

	if [ "${NANO_MD_BACKING}" = "swap" ] ; then
		if [ ${NANO_IMAGE_MBRONLY} ]; then
			echo "Writing out _.disk.mbr..."
			dd if=/dev/${MD} of=${NANO_DISKIMGDIR}/_.disk.mbr bs=512 count=1
		else
			echo "Writing out ${NANO_IMGNAME}..."
			dd if=/dev/${MD} of=${IMG} bs=64k
		fi

		echo "Writing out ${NANO_IMGNAME}..."
		dd conv=sparse if=/dev/${MD} of=${IMG} bs=64k
	fi

	mdconfig -d -u $MD

	trap - 1 2 15 EXIT

	) > ${NANO_LOG}/_.di 2>&1
)
