#!/bin/sh

passphrase=passphrase
iterations=50000

# The smallest FAT32 filesystem is 33292 KB
espsize=33292

#
# Builds all the bat-shit crazy combinations we support booting from,
# at least for amd64. It assume you have a ~sane kernel in /boot/kernel
# and copies that into the ~150MB root images we create (we create the du
# size of the kernel + 20MB).
#
# Sad panda sez: this runs as root, but could be any user if someone
# creates userland geli.
#
# This assumes an external program install-boot.sh which will install
# the appropriate boot files in the appropriate locations.
#
# Assumes you've already rebuilt... maybe bad? Also maybe bad: the env
# vars should likely be conditionally set to allow better automation.
#

. $(dirname $0)/install-boot.sh

cpsys() {
    src=$1
    dst=$2

    # Copy kernel + boot loader
    (cd $src ; tar cf - .) | (cd $dst; tar xf -)
}

ufs_fstab() {
    dir=$1

    cat > ${dir}/etc/fstab <<EOF
/dev/ufs/root	/		ufs	rw	1	1
EOF
}

mk_nogeli_gpt_ufs_legacy() {
    src=$1
    img=$2

    ufs_fstab ${src}
    makefs -t ffs -B little -s 200m -o label=root ${img}.p2 ${src}
    mkimg -s gpt -b ${src}/boot/pmbr \
	  -p freebsd-boot:=${src}/boot/gptboot \
	  -p freebsd-ufs:=${img}.p2 -o ${img}
    rm -f ${src}/etc/fstab
}

mk_nogeli_gpt_ufs_uefi() {
    src=$1
    img=$2

    ufs_fstab ${src}
    make_esp_file ${img}.p1 ${espsize} ${src}/boot/loader.efi
    makefs -t ffs -B little -s 200m -o label=root ${img}.p2 ${src}
    mkimg -s gpt \
	  -p efi:=${img}.p1 \
	  -p freebsd-ufs:=${img}.p2 -o ${img}
    rm -f ${src}/etc/fstab
}

mk_nogeli_gpt_ufs_both() {
    src=$1
    img=$2

    ufs_fstab ${src}
    make_esp_file ${img}.p1 ${espsize} ${src}/boot/loader.efi
    makefs -t ffs -B little -s 200m -o label=root ${img}.p3 ${src}
    # p1 is boot for uefi, p2 is boot for gpt, p3 is /
    mkimg -b ${src}/boot/pmbr -s gpt \
	  -p efi:=${img}.p1 \
	  -p freebsd-boot:=${src}/boot/gptboot \
	  -p freebsd-ufs:=${img}.p3 \
	  -o ${img}
    rm -f ${src}/etc/fstab
}

# XXX should not assume host == target
zfs_extra()
{
    src=$1
    dst=$2

    mkdir -p $dst
    mkdir -p $dst/boot/kernel
    cat > ${dst}/boot/loader.conf.local <<EOF
cryptodev_load=YES
zfs_load=YES
EOF
    cp /boot/kernel/acl_nfs4.ko ${dst}/boot/kernel/acl_nfs4.ko
    cp /boot/kernel/cryptodev.ko ${dst}/boot/kernel/cryptodev.ko
    cp /boot/kernel/zfs.ko ${dst}/boot/kernel/zfs.ko
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
    dst=$img.extra

    zfs_extra $src $dst
    makefs -t zfs -s 200m \
	-o poolname=${pool} -o bootfs=${pool} -o rootpath=/ \
	${img}.p2 ${src} ${dst}
    mkimg -b ${src}/boot/pmbr -s gpt \
	  -p freebsd-boot:=/boot/gptzfsboot \
	  -p freebsd-zfs:=${img}.p2 \
	  -o ${img}
    rm -rf ${dst}
}

mk_nogeli_gpt_zfs_uefi() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7
    pool=nogeli-gpt-zfs-uefi
    dst=$img.extra

    zfs_extra $src $dst
    make_esp_file ${img}.p1 ${espsize} ${src}/boot/loader.efi
    makefs -t zfs -s 200m \
	-o poolname=${pool} -o bootfs=${pool} -o rootpath=/ \
	${img}.p2 ${src} ${dst}
    mkimg -b ${src}/boot/pmbr -s gpt \
	  -p efi:=${img}.p1 \
	  -p freebsd-zfs:=${img}.p2 \
	  -o ${img}
    rm -rf ${dst}
}

mk_nogeli_gpt_zfs_both() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7
    pool=nogeli-gpt-zfs-both
    dst=$img.extra

    zfs_extra $src $dst
    make_esp_file ${img}.p2 ${espsize} ${src}/boot/loader.efi
    makefs -t zfs -s 200m \
	-o poolname=${pool} -o bootfs=${pool} -o rootpath=/ \
	${img}.p3 ${src} ${dst}
    mkimg -b ${src}/boot/pmbr -s gpt \
	  -p freebsd-boot:=/boot/gptzfsboot \
	  -p efi:=${img}.p2 \
	  -p freebsd-zfs:=${img}.p3 \
	  -o ${img}
    rm -rf ${dst}
}

mk_nogeli_mbr_ufs_legacy() {
    src=$1
    img=$2

    ufs_fstab ${src}
    makefs -t ffs -B little -s 200m -o label=root ${img}.s1a ${src}
    mkimg -s bsd -b ${src}/boot/boot -p freebsd-ufs:=${img}.s1a -o ${img}.s1
    mkimg -a 1 -s mbr -b ${src}/boot/boot0sio -p freebsd:=${img}.s1 -o ${img}
    rm -f ${src}/etc/fstab
}

mk_nogeli_mbr_ufs_uefi() {
    src=$1
    img=$2

    ufs_fstab ${src}
    make_esp_file ${img}.s1 ${espsize} ${src}/boot/loader.efi
    makefs -t ffs -B little -s 200m -o label=root ${img}.s2a ${src}
    mkimg -s bsd -p freebsd-ufs:=${img}.s2a -o ${img}.s2
    mkimg -a 1 -s mbr -p efi:=${img}.s1 -p freebsd:=${img}.s2 -o ${img}
    rm -f ${src}/etc/fstab
}

mk_nogeli_mbr_ufs_both() {
    src=$1
    img=$2

    ufs_fstab ${src}
    make_esp_file ${img}.s1 ${espsize} ${src}/boot/loader.efi
    makefs -t ffs -B little -s 200m -o label=root ${img}.s2a ${src}
    mkimg -s bsd -b ${src}/boot/boot -p freebsd-ufs:=${img}.s2a -o ${img}.s2
    mkimg -a 2 -s mbr -b ${src}/boot/mbr -p efi:=${img}.s1 -p freebsd:=${img}.s2 -o ${img}
    rm -f ${src}/etc/fstab
}

mk_nogeli_mbr_zfs_legacy() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7
    pool=nogeli-mbr-zfs-legacy

    zfs_extra $src $dst
    makefs -t zfs -s 200m \
	-o poolname=${pool} -o bootfs=${pool} -o rootpath=/ \
	${img}.s1a ${src} ${dst}
    # The old boot1/boot2 boot split is also used by zfs. We need to extract zfsboot1
    # from this image. Since there's no room in the mbr format for the rest of the loader,
    # it will load the zfsboot loader from the reserved for bootloader area of the ZFS volume
    # being booted, hence the need to dd it into the raw img later.
    # Please note: zfsboot only works with partition 'a' which must be the root
    # partition / zfs volume
    dd if=${src}/boot/zfsboot of=${dst}/zfsboot1 count=1
    mkimg -s bsd -b ${dst}zfsboot1 -p freebsd-zfs:=${img}.s1a -o ${img}.s1
    dd if=${src}/boot/zfsboot of=${img}.s1a skip=1 seek=1024
    mkimg -a 1 -s mbr -b ${src}/boot/mbr -p freebsd:=${img}.s1 -o ${img}
    rm -rf ${dst}
}

mk_nogeli_mbr_zfs_uefi() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7
    pool=nogeli-mbr-zfs-uefi

    zfs_extra $src $dst
    make_esp_file ${img}.s1 ${espsize} ${src}/boot/loader.efi
    makefs -t zfs -s 200m \
	-o poolname=${pool} -o bootfs=${pool} -o rootpath=/ \
	${img}.s2a ${src} ${dst}
    mkimg -s bsd -b ${dst}zfsboot1 -p freebsd-zfs:=${img}.s2a -o ${img}.s2
    mkimg -a 1 -s mbr -b ${src}/boot/mbr -p efi:=${img}.s1 -p freebsd:=${img}.s2 -o ${img}
    rm -rf ${dst}
}

mk_nogeli_mbr_zfs_both() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7
    pool=nogeli-mbr-zfs-both

    zfs_extra $src $dst
    make_esp_file ${img}.s1 ${espsize} ${src}/boot/loader.efi
    makefs -t zfs -s 200m \
	-o poolname=${pool} -o bootfs=${pool} -o rootpath=/ \
	${img}.s2a ${src} ${dst}
    # The old boot1/boot2 boot split is also used by zfs. We need to extract zfsboot1
    # from this image. Since there's no room in the mbr format for the rest of the loader,
    # it will load the zfsboot loader from the reserved for bootloader area of the ZFS volume
    # being booted, hence the need to dd it into the raw img later.
    # Please note: zfsboot only works with partition 'a' which must be the root
    # partition / zfs volume
    dd if=${src}/boot/zfsboot of=${dst}/zfsboot1 count=1
    mkimg -s bsd -b ${dst}zfsboot1 -p freebsd-zfs:=${img}.s2a -o ${img}.s2
    dd if=${src}/boot/zfsboot of=${img}.s1a skip=1 seek=1024
    mkimg -a 1 -s mbr -b ${src}/boot/mbr -p efi:=${img}.s1 -p freebsd:=${img}.s2 -o ${img}
}

mk_geli_gpt_ufs_legacy() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7

    dd if=/dev/zero of=${img} count=1 seek=$(( 200 * 1024 * 1024 / 512 ))
    md=$(mdconfig -f ${img})
    gpart create -s gpt ${md}
    gpart add -t freebsd-boot -s 400k -a 4k	${md}	# <= ~540k
    gpart add -t freebsd-ufs -l root $md
    # install-boot will make this bootable
    echo ${passphrase} | geli init -bg -e AES-XTS -i ${iterations} -J - -l 256 -s 4096 ${md}p2
    echo ${passphrase} | geli attach -j - ${md}p2
    newfs -L root /dev/${md}p2.eli
    mount /dev/${md}p2.eli ${mntpt}
    cpsys ${src} ${mntpt}
    # need to make a couple of tweaks
    cat > ${mntpt}/boot/loader.conf <<EOF
geom_eli_load=YES
EOF
    ufs_fstab ${mntpt}

    cp /boot/kernel/geom_eli.ko ${mntpt}/boot/kernel/geom_eli.ko
    # end tweaks
    umount -f ${mntpt}
    geli detach ${md}p2
    ${SRCTOP}/tools/boot/install-boot.sh -g ${geli} -s ${scheme} -f ${fs} -b ${bios} -d ${src} ${md}
    mdconfig -d -u ${md}
}

mk_geli_gpt_ufs_uefi() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7

    dd if=/dev/zero of=${img} count=1 seek=$(( 200 * 1024 * 1024 / 512 ))
    md=$(mdconfig -f ${img})
    gpart create -s gpt ${md}
    gpart add -t efi -s ${espsize}k -a 4k ${md}
    gpart add -t freebsd-ufs -l root $md
    # install-boot will make this bootable
    echo ${passphrase} | geli init -bg -e AES-XTS -i ${iterations} -J - -l 256 -s 4096 ${md}p2
    echo ${passphrase} | geli attach -j - ${md}p2
    newfs -L root /dev/${md}p2.eli
    mount /dev/${md}p2.eli ${mntpt}
    cpsys ${src} ${mntpt}
    # need to make a couple of tweaks
    cat > ${mntpt}/boot/loader.conf <<EOF
geom_eli_load=YES
EOF
    ufs_fstab ${mntpt}

    cp /boot/kernel/geom_eli.ko ${mntpt}/boot/kernel/geom_eli.ko
    # end tweaks
    umount -f ${mntpt}
    geli detach ${md}p2
    ${SRCTOP}/tools/boot/install-boot.sh -g ${geli} -s ${scheme} -f ${fs} -b ${bios} -d ${src} ${md}
    mdconfig -d -u ${md}
}

mk_geli_gpt_ufs_both() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7

    dd if=/dev/zero of=${img} count=1 seek=$(( 200 * 1024 * 1024 / 512 ))
    md=$(mdconfig -f ${img})
    gpart create -s gpt ${md}
    gpart add -t efi -s ${espsize}k -a 4k ${md}
    gpart add -t freebsd-boot -s 400k -a 4k	${md}	# <= ~540k
    gpart add -t freebsd-ufs -l root $md
    # install-boot will make this bootable
    echo ${passphrase} | geli init -bg -e AES-XTS -i ${iterations} -J - -l 256 -s 4096 ${md}p3
    echo ${passphrase} | geli attach -j - ${md}p3
    newfs -L root /dev/${md}p3.eli
    mount /dev/${md}p3.eli ${mntpt}
    cpsys ${src} ${mntpt}
    # need to make a couple of tweaks
    cat > ${mntpt}/boot/loader.conf <<EOF
geom_eli_load=YES
EOF
    ufs_fstab ${mntpt}

    cp /boot/kernel/geom_eli.ko ${mntpt}/boot/kernel/geom_eli.ko
    # end tweaks
    umount -f ${mntpt}
    geli detach ${md}p3
    ${SRCTOP}/tools/boot/install-boot.sh -g ${geli} -s ${scheme} -f ${fs} -b ${bios} -d ${src} ${md}
    mdconfig -d -u ${md}
}

mk_geli_gpt_zfs_legacy() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7
    pool=geli-gpt-zfs-legacy

    # Note that in this flavor we create an empty p2 ufs partition, and put
    # the bootable zfs stuff on p3, just to test the ability of the zfs probe
    # probe routines to find a pool on a partition other than the first one.

    dd if=/dev/zero of=${img} count=1 seek=$(( 300 * 1024 * 1024 / 512 ))
    md=$(mdconfig -f ${img})
    gpart create -s gpt ${md}
    gpart add -t freebsd-boot -s 400k -a 4k	${md}	# <= ~540k
    gpart add -t freebsd-ufs -s 100m ${md}
    gpart add -t freebsd-zfs -l root $md
    # install-boot will make this bootable
    echo ${passphrase} | geli init -bg -e AES-XTS -i ${iterations} -J - -l 256 -s 4096 ${md}p3
    echo ${passphrase} | geli attach -j - ${md}p3
    zpool create -O mountpoint=none -R ${mntpt} ${pool} ${md}p3.eli
    zpool set bootfs=${pool} ${pool}
    zfs create -po mountpoint=/ ${pool}/ROOT/default
    # NB: The online guides go nuts customizing /var and other mountpoints here, no need
    cpsys ${src} ${mntpt}
    # need to make a couple of tweaks
    cat >> ${mntpt}/boot/loader.conf <<EOF
cryptodev_load=YES
zfs_load=YES
geom_eli_load=YES
EOF
    cp /boot/kernel/acl_nfs4.ko ${mntpt}/boot/kernel/acl_nfs4.ko
    cp /boot/kernel/cryptodev.ko ${mntpt}/boot/kernel/cryptodev.ko
    cp /boot/kernel/zfs.ko ${mntpt}/boot/kernel/zfs.ko
    cp /boot/kernel/geom_eli.ko ${mntpt}/boot/kernel/geom_eli.ko
    # end tweaks
    zfs umount -f ${pool}/ROOT/default
    zfs set mountpoint=none ${pool}/ROOT/default
    zpool set bootfs=${pool}/ROOT/default ${pool}
    zpool set autoexpand=on ${pool}
    zpool export ${pool}
    geli detach ${md}p3
    ${SRCTOP}/tools/boot/install-boot.sh -g ${geli} -s ${scheme} -f ${fs} -b ${bios} -d ${src} ${md}
    mdconfig -d -u ${md}
}

mk_geli_gpt_zfs_uefi() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7
    pool=geli-gpt-zfs-uefi

    # Note that in this flavor we create an empty p2 ufs partition, and put
    # the bootable zfs stuff on p3, just to test the ability of the zfs probe
    # probe routines to find a pool on a partition other than the first one.

    dd if=/dev/zero of=${img} count=1 seek=$(( 300 * 1024 * 1024 / 512 ))
    md=$(mdconfig -f ${img})
    gpart create -s gpt ${md}
    gpart add -t efi -s ${espsize}k -a 4k ${md}
    gpart add -t freebsd-ufs -s 100m ${md}
    gpart add -t freebsd-zfs -l root $md
    # install-boot will make this bootable
    echo ${passphrase} | geli init -bg -e AES-XTS -i ${iterations} -J - -l 256 -s 4096 ${md}p3
    echo ${passphrase} | geli attach -j - ${md}p3
    zpool create -O mountpoint=none -R ${mntpt} ${pool} ${md}p3.eli
    zpool set bootfs=${pool} ${pool}
    zfs create -po mountpoint=/ ${pool}/ROOT/default
    # NB: The online guides go nuts customizing /var and other mountpoints here, no need
    cpsys ${src} ${mntpt}
    # need to make a couple of tweaks
    cat >> ${mntpt}/boot/loader.conf <<EOF
cryptodev_load=YES
zfs_load=YES
geom_eli_load=YES
EOF
    cp /boot/kernel/acl_nfs4.ko ${mntpt}/boot/kernel/acl_nfs4.ko
    cp /boot/kernel/cryptodev.ko ${mntpt}/boot/kernel/cryptodev.ko
    cp /boot/kernel/zfs.ko ${mntpt}/boot/kernel/zfs.ko
    cp /boot/kernel/geom_eli.ko ${mntpt}/boot/kernel/geom_eli.ko
    # end tweaks
    zfs umount -f ${pool}/ROOT/default
    zfs set mountpoint=none ${pool}/ROOT/default
    zpool set bootfs=${pool}/ROOT/default ${pool}
    zpool set autoexpand=on ${pool}
    zpool export ${pool}
    geli detach ${md}p3
    ${SRCTOP}/tools/boot/install-boot.sh -g ${geli} -s ${scheme} -f ${fs} -b ${bios} -d ${src} ${md}
    mdconfig -d -u ${md}
}

mk_geli_gpt_zfs_both() {
    src=$1
    img=$2
    mntpt=$3
    geli=$4
    scheme=$5
    fs=$6
    bios=$7
    pool=geli-gpt-zfs-both

    dd if=/dev/zero of=${img} count=1 seek=$(( 200 * 1024 * 1024 / 512 ))
    md=$(mdconfig -f ${img})
    gpart create -s gpt ${md}
    gpart add -t efi -s ${espsize}k -a 4k ${md}
    gpart add -t freebsd-boot -s 400k -a 4k	${md}	# <= ~540k
    gpart add -t freebsd-zfs -l root $md
    # install-boot will make this bootable
    echo ${passphrase} | geli init -bg -e AES-XTS -i ${iterations} -J - -l 256 -s 4096 ${md}p3
    echo ${passphrase} | geli attach -j - ${md}p3
    zpool create -O mountpoint=none -R ${mntpt} ${pool} ${md}p3.eli
    zpool set bootfs=${pool} ${pool}
    zfs create -po mountpoint=/ ${pool}/ROOT/default
    # NB: The online guides go nuts customizing /var and other mountpoints here, no need
    cpsys ${src} ${mntpt}
    # need to make a couple of tweaks
    cat > ${mntpt}/boot/loader.conf <<EOF
cryptodev_load=YES
zfs_load=YES
geom_eli_load=YES
EOF
    cp /boot/kernel/acl_nfs4.ko ${mntpt}/boot/kernel/acl_nfs4.ko
    cp /boot/kernel/cryptodev.ko ${mntpt}/boot/kernel/cryptodev.ko
    cp /boot/kernel/zfs.ko ${mntpt}/boot/kernel/zfs.ko
    cp /boot/kernel/geom_eli.ko ${mntpt}/boot/kernel/geom_eli.ko
    # end tweaks
    zfs umount -f ${pool}/ROOT/default
    zfs set mountpoint=none ${pool}/ROOT/default
    zpool set bootfs=${pool}/ROOT/default ${pool}
    zpool set autoexpand=on ${pool}
    zpool export ${pool}
    geli detach ${md}p3
    ${SRCTOP}/tools/boot/install-boot.sh -g ${geli} -s ${scheme} -f ${fs} -b ${bios} -d ${src} ${md}
    mdconfig -d -u ${md}
}

# GELI+MBR is not a valid configuration
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
# powerpc

qser="-monitor telnet::4444,server,nowait -serial stdio -nographic"

# https://wiki.freebsd.org/QemuRecipes
# aarch64
qemu_aarch64_uefi()
{
    img=$1
    sh=$2

    echo "qemu-system-aarch64 -m 4096M -cpu cortex-a57 -M virt  \
        -bios QEMU_EFI.fd ${qser} \
        -drive if=none,file=${img},id=hd0 \
        -device virtio-blk-device,drive=hd0" > $sh
    chmod 755 $sh
# https://wiki.freebsd.org/arm64/QEMU also has
#       -device virtio-net-device,netdev=net0
#       -netdev user,id=net0
}

log_for()
{
    dir=$(dirname $1)
    fn=$(basename $1 .sh)
    echo $dir/$fn.log
}

# Amd64 qemu
qemu_amd64_legacy()
{
    img=$1
    sh=$2
    log=$(log_for $2)

    echo "echo -n $(basename $sh .sh):' '" > $sh
    echo "(qemu-system-x86_64 -m 256m --drive file=${img},format=raw ${qser} | tee $log 2>&1 | grep -q SUCCESS) && echo legacy pass || echo legacy fail" >> $sh
    chmod 755 $sh
}

qemu_amd64_uefi()
{
    img=$1
    sh=$2
    log=$(log_for $2)

    echo "echo -n $(basename $sh .sh):' '" > $sh
    echo "(qemu-system-x86_64 -m 256m -bios ~/bios/OVMF-X64.fd --drive file=${img},format=raw ${qser} | tee $log 2>&1 | grep -q SUCCESS) && echo uefi pass || echo uefi fail" >> $sh
    chmod 755 $sh
}

qemu_amd64_both()
{
    img=$1
    sh=$2
    log=$(log_for $2)

    echo "echo -n $(basename $sh .sh):' '" > $sh
    echo "(qemu-system-x86_64 -m 256m --drive file=${img},format=raw ${qser} | tee $log 2>&1 | grep -q SUCCESS) && echo legacy pass || echo legacy fail" >> $sh
    echo "echo -n $(basename $sh .sh):' '" >> $sh
    echo "(qemu-system-x86_64 -m 256m -bios ~/bios/OVMF-X64.fd --drive file=${img},format=raw ${qser} | tee -a $log 2>&1 | grep -q SUCCESS) && echo uefi pass || echo uefi fail" >> $sh
    chmod 755 $sh
}

# arm
# nothing listed?

# i386
qemu_i386_legacy()
{
    img=$1
    sh=$2

    echo "qemu-system-i386 --drive file=${img},format=raw ${qser}" > $sh
    chmod 755 $sh
}

# Not yet supported
qemu_i386_uefi()
{
    img=$1
    sh=$2

    echo "qemu-system-i386 -bios ~/bios/OVMF-X32.fd --drive file=${img},format=raw ${qser}" > $sh
    chmod 755 $sh
}

# Needs UEFI to be supported
qemu_i386_both()
{
    img=$1
    sh=$2

    echo "qemu-system-i386 --drive file=${img},format=raw ${qser}" > $sh
    echo "qemu-system-i386 -bios ~/bios/OVMF-X32.fd --drive file=${img},format=raw ${qser}" >> $sh
    chmod 755 $sh
}

make_one_image()
{
    local arch=${1?}
    local geli=${2?}
    local scheme=${3?}
    local fs=${4?}
    local bios=${5?}

    # Create sparse file and mount newly created filesystem(s) on it
    img=${IMGDIR}/${arch}-${geli}-${scheme}-${fs}-${bios}.img
    sh=${IMGDIR}/${arch}-${geli}-${scheme}-${fs}-${bios}.sh
    echo "$sh" >> ${IMGDIR}/all.sh
    echo date >> ${IMGDIR}/all.sh
    echo "vvvvvvvvvvvvvv   Creating $img  vvvvvvvvvvvvvvv"
    rm -f ${img}*
    eval mk_${geli}_${scheme}_${fs}_${bios} ${DESTDIR} ${img} ${MNTPT} ${geli} ${scheme} ${fs} ${bios}
    eval qemu_${arch}_${bios} ${img} ${sh}
    [ -n "${SUDO_USER}" ] && chown ${SUDO_USER} ${img}*
    echo "^^^^^^^^^^^^^^   Created $img   ^^^^^^^^^^^^^^^"
}

# Powerpc -- doesn't work but maybe it would enough for testing -- needs details
# powerpc64
# qemu-system-ppc64 -drive file=/path/to/disk.img,format=raw

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
echo -h -D -S115200 > ${DESTDIR}/boot.config
cat > ${DESTDIR}/boot/loader.conf <<EOF
comconsole_speed=115200
autoboot_delay=0
EOF
# XXX
cp /boot/device.hints ${DESTDIR}/boot/device.hints
# Assume we're already built
make install DESTDIR=${DESTDIR} MK_MAN=no MK_INSTALL_AS_USER=yes WITHOUT_DEBUG_FILES=yes
if [ $? -ne 0 ]; then
        echo "make install failed"
        exit 1
fi
# Copy init, /bin/sh, minimal libraries and testing /etc/rc
mkdir -p ${DESTDIR}/sbin ${DESTDIR}/bin \
      ${DESTDIR}/lib ${DESTDIR}/libexec \
      ${DESTDIR}/etc ${DESTDIR}/dev
for f in /sbin/halt /sbin/init /bin/sh /sbin/sysctl $(ldd /bin/sh | awk 'NF == 4 { print $3; }') /libexec/ld-elf.so.1; do
    cp $f ${DESTDIR}/$f
done
cat > ${DESTDIR}/etc/rc <<EOF
#!/bin/sh

sysctl machdep.bootmethod
echo "RC COMMAND RUNNING -- SUCCESS!!!!!"
halt -p
EOF

# If we were given exactly 5 args, go make that one image.

rm -f ${IMGDIR}/all.sh
echo date > ${IMGDIR}/all.sh
chmod +x  ${IMGDIR}/all.sh

if [ $# -eq 5 ]; then
    make_one_image $*
    echo ${IMGDIR}/all.sh
    exit
fi

# OK. Let the games begin

for arch in amd64; do
    for geli in nogeli; do # geli
	for scheme in gpt mbr; do
	    for fs in ufs zfs; do
		for bios in legacy uefi both; do
		    make_one_image ${arch} ${geli} ${scheme} ${fs} ${bios}
		done
	    done
	done
    done
done
    # We should also do a cd image for amd64 here
echo ${IMGDIR}/all.sh

rmdir ${MNTPT}

exit 0

# Notes for the future

for arch in i386; do
    for geli in nogeli geli; do
	for scheme in gpt mbr; do
	    for fs in ufs zfs; do
		for bios in legacy; do
		    # The legacy boot is shared with amd64 so those routines could
		    # likely be used here.
		    make_one_image ${arch} ${geli} ${scheme} ${fs} ${bios}
		done
	    done
	done
    done
done
    # We should also do a cd image for i386 here

for arch in arm aarch64; do
    geli=nogeli		# I don't think geli boot works / is supported on arm
    for scheme in gpt mbr; do
      for fs in ufs zfs; do
	bios=efi # Note: arm has some uboot support with ufs, what to do?
	make_one_image ${arch} ${geli} ${scheme} ${fs} ${bios}
      done
    done
done

# It's not clear that the nested looping paradigm is best for powerpc
# due to its diversity.
for arch in powerpc powerpc64 powerpc64le; do
    geli=nogeli
    for scheme in apm gpt; do
	fs=ufs # zfs + gpt might be supported?
	for bios in ofw uboot chrp; do
	    make_one_image ${arch} ${geli} ${scheme} ${fs} ${bios}
	done
    done
done

for arch in riscv; do
    geli=nogeli
    fs=ufs		# Generic ZFS booting support with efi?
    scheme=gpt
    bios=efi
    make_one_image ${arch} ${geli} ${scheme} ${fs} ${bios}
done
