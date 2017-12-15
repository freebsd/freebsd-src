#!/bin/sh

# $FreeBSD$

#
# Builds all the bat-shit crazy combinations we support booting from,
# at least for amd64. It assume you have a ~sane kernel in /boot/kernel
# and copies that into the ~150MB root images we create (we create the du
# size of the kernel + 20MB
#
# Sad panda sez: this runs as root, but could be userland if someone
# creates userland geli and zfs tools.
#
# This assumes an external prograam install-boot.sh which will install
# the appropriate boot files in the appropriate locations.
#
# These images assume ada0 will be the root image. We should likely
# use labels, but we don't.
#
# ASsumes you've already rebuilt... maybe bad? Also maybe bad: the env
# vars should likely be conditionally set to allow better automation.
#

cpsys() {
    src=$1
    dst=$2

    # Copy kernel + boot loader
    (cd $src ; tar cf - .) | (cd $dst; tar xf -)
}

mk_nogeli_gpt_ufs_legacy() {
    src=$1
    img=$2

    rm -f ${img} ${img}.p2
    makefs -t ffs -B little -s 200m ${img}.p2 ${src}
    mkimg -s gpt -b ${src}/boot/pmbr \
	  -p freebsd-boot:=${src}/boot/gptboot \
	  -p freebsd-ufs:=${img}.p2 -o ${img}
}

mk_nogeli_gpt_ufs_uefi() {
    src=$1
    img=$2

    rm -f ${img} ${img}.p2
    makefs -t ffs -B little -s 200m ${img}.p2 ${src}
    mkimg -s gpt -b ${src}/boot/pmbr \
	  -p freebsd-boot:=${src}/boot/gptboot \
	  -p freebsd-ufs:=${img}.p2 -o ${img}
}

mk_nogeli_gpt_ufs_both() {
    src=$1
    img=$2

    makefs -t ffs -B little -s 200m ${img}.p3 ${src}
    # p1 is boot for uefi, p2 is boot for gpt, p3 is /
    mkimg -b ${src}/boot/pmbr -s gpt \
	  -p efi:=${src}/boot/boot1.efifat \
	  -p freebsd-boot:=${src}/boot/gptboot \
	  -p freebsd-ufs:=${img}.p3 \
	  -o ${img}
}

mk_nogeli_gpt_zfs_legacy() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7
    pool=nogeli-gpt-zfs-legacy

    rm -f ${img}
    dd if=/dev/zero of=${img} count=1 seek=$((200 * 1024 * 1024 / 512))
    md=$(mdconfig -f ${img})
    gpart create -s gpt ${md}
    gpart add -t freebsd-boot -s 400k -a 4k	${md}	# <= ~540k
    gpart add -t freebsd-zfs -l root $md
    # install-boot will make this bootable
    zpool create -O mountpoint=none -R ${mntpt} ${pool} ${md}p2
    zpool set bootfs=${pool} ${pool}
    zfs create -o mountpoint=/ ${pool}/ROOT
    # NB: The online guides go nuts customizing /var and other mountpoints here, no need
    cpsys ${src} ${mntpt}
    df
    # need to make a couple of tweaks
    cat > ${mntpt}/boot/loader.conf <<EOF
zfs_load=YES
opensolaris_load=YES
EOF
    cp /boot/kernel/zfs.ko ${mntpt}/boot/kernel/zfs.ko
    cp /boot/kernel/opensolaris.ko ${mntpt}/boot/kernel/opensolaris.ko
    # end tweaks
    zfs umount -f ${pool}/ROOT
    zfs set mountpoint=none ${pool}/ROOT
    zpool set bootfs=${pool}/ROOT ${pool}
    zpool set autoexpand=on ${pool}
    zpool export ${pool}
    ${SRCTOP}/tools/boot/install-boot.sh -g ${geli} -s ${scheme} -f ${fs} -b ${bios} -d ${src} ${md}
    mdconfig -d -u ${md}
}

mk_nogeli_gpt_zfs_uefi() {
}

mk_nogeli_gpt_zfs_both() {
}

mk_nogeli_mbr_ufs_legacy() {
}

mk_nogeli_mbr_ufs_uefi() {
}

mk_nogeli_mbr_ufs_both() {
}

mk_nogeli_mbr_zfs_legacy() {
}

mk_nogeli_mbr_zfs_uefi() {
}

mk_nogeli_mbr_zfs_both() {
}

mk_geli_gpt_ufs_legacy() {
}

mk_geli_gpt_ufs_uefi() {
}

mk_geli_gpt_ufs_both() {
}

mk_geli_gpt_zfs_legacy() {
}

mk_geli_gpt_zfs_uefi() {
}

mk_geli_gpt_zfs_both() {
}

mk_geli_mbr_ufs_legacy() {
}

mk_geli_mbr_ufs_uefi() {
}

mk_geli_mbr_ufs_both() {
}

mk_geli_mbr_zfs_legacy() {
}

mk_geli_mbr_zfs_uefi() {
}

mk_geli_mbr_zfs_both() {
}

# iso
# pxeldr
# u-boot

# Misc variables
SRCTOP=$(make -v SRCTOP)
cd ${SRCTOP}/stand
OBJDIR=$(make -v .OBJDIR)
IMGDIR=${OBJDIR}/boot-images
mkdir -p ${IMGDIR}
MNTPT=$(mktemp -d /tmp/stand-test.XXXXXX)

# Setup the installed tree...
DESTDIR=${OBJDIR}/boot-tree
rm -rf ${DESTDIR}
mkdir -p ${DESTDIR}/boot/defaults
mkdir -p ${DESTDIR}/boot/kernel
cp /boot/kernel/kernel ${DESTDIR}/boot/kernel
echo -D -S115200 > ${DESTDIR}/boot.config
# XXX
cp /boot/device.hints ${DESTDIR}/boot/device.hints
# Assume we're already built
make install DESTDIR=${DESTDIR} MK_MAN=no MK_INSTALL_AS_USER=yes
# Copy init, /bin/sh and minimal libraries
mkdir -p ${DESTDIR}/sbin ${DESTDIR}/bin ${DESTDIR}/lib ${DESTDIR}/libexec
for f in /sbin/init /bin/sh $(ldd /bin/sh | awk 'NF == 4 { print $3; }') /libexec/ld-elf.so.1; do
    cp $f ${DESTDIR}/$f
done
mkdir ${DESTDIR}/dev

# OK. Let the games begin

for geli in nogeli geli; do
    for scheme in gpt mbr; do
	for fs in ufs zfs; do
	    for bios in legacy uefi both; do
		# Create sparse file and mount newly created filesystem(s) on it
		img=${IMGDIR}/${geli}-${scheme}-${fs}-${bios}.img
		echo "vvvvvvvvvvvvvvvvvvvvvv   Creating $img  vvvvvvvvvvvvvvvvvvvvvvv"
		eval mk_${geli}_${scheme}_${fs}_${bios} ${DESTDIR} ${img} ${MNTPT} ${geli} ${scheme} ${fs} ${bios}
		echo "^^^^^^^^^^^^^^^^^^^^^^   Creating $img  ^^^^^^^^^^^^^^^^^^^^^^^"
	    done
	done
    done
done

rmdir ${MNTPT}
