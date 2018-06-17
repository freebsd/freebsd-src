#!/bin/sh

# $FreeBSD$

#
# Installs/updates the necessary boot blocks for the desired boot environment
#
# Lightly tested.. Intended to be installed, but until it matures, it will just
# be a boot tool for regression testing.

# insert code here to guess what you have -- yikes!

die() {
    echo $*
    exit 1
}

doit() {
    echo $*
    eval $*
}

find-part() {
    dev=$1
    part=$2

    gpart show $dev | tail +2 | awk '$4 == "'$part'" { print $3; }'
}

boot_nogeli_gpt_ufs_legacy() {
    dev=$1
    dst=$2

    idx=$(find-part $dev "freebsd-boot")
    if [ -z "$idx" ] ; then
	die "No freebsd-boot partition found"
    fi
    doit gpart bootcode -b ${gpt0} -p ${gpt2} -i $idx $dev
}

boot_nogeli_gpt_ufs_uefi() {
    dev=$1
    dst=$2

    idx=$(find-part $dev "efi")
    if [ -z "$idx" ] ; then
	die "No ESP partition found"
    fi
    doit gpart bootcode -p ${efi2} -i $idx $dev
}

boot_nogeli_gpt_ufs_both() {
    boot_nogeli_gpt_ufs_legacy $1 $2 $3
    boot_nogeli_gpt_ufs_uefi $1 $2 $3
}

boot_nogeli_gpt_zfs_legacy() {
    dev=$1
    dst=$2

    idx=$(find-part $dev "freebsd-boot")
    if [ -z "$idx" ] ; then
	die "No freebsd-boot partition found"
    fi
    doit gpart bootcode -b ${gpt0} -p ${gptzfs2} -i $idx $dev
}

boot_nogeli_gpt_zfs_uefi() {
    dev=$1
    dst=$2

    idx=$(find-part $dev "efi")
    if [ -z "$idx" ] ; then
	die "No ESP partition found"
    fi
    doit gpart bootcode -p ${efi2} -i $idx $dev
}

boot_nogeli_gpt_zfs_both() {
    boot_nogeli_gpt_zfs_legacy $1 $2 $3
    boot_nogeli_gpt_zfs_uefi $1 $2 $3
}

boot_nogeli_mbr_ufs_legacy() {
    dev=$1
    dst=$2

    doit gpart bootcode -b ${mbr0} ${dev}
    s=$(find-part $dev "freebsd")
    if [ -z "$s" ] ; then
	die "No freebsd slice found"
    fi
    doit gpart bootcode -p ${mbr2} ${dev}s${s}
}

boot_nogeli_mbr_ufs_uefi() {
    dev=$1
    dst=$2

    s=$(find-part ${dev} "!239")
    if [ -z "$s" ] ; then
	die "No ESP slice found"
    fi
    doit gpart bootcode -p ${efi2} -i ${s} ${dev}
}

boot_nogeli_mbr_ufs_both() {
    boot_nogeli_mbr_ufs_legacy $1 $2 $3
    boot_nogeli_mbr_ufs_uefi $1 $2 $3
}

boot_nogeli_mbr_zfs_legacy() {
    dev=$1
    dst=$2

    # search to find the BSD slice
    s=$(find-part $dev "freebsd")
    if [ -z "$s" ] ; then
	die "No BSD slice found"
    fi
    idx=$(find-part ${dev}s${s} "freebsd-zfs")
    if [ -z "$idx" ] ; then
	die "No freebsd-zfs slice found"
    fi
    # search to find the freebsd-zfs partition within the slice
    # Or just assume it is 'a' because it has to be since it fails otherwise
    doit gpart bootcode -b ${dst}/boot/mbr ${dev}
    dd if=${dst}/boot/zfsboot of=/tmp/zfsboot1 count=1
    doit gpart bootcode -b /tmp/zfsboot1 ${dev}s${s}	# Put boot1 into the start of part
    sysctl kern.geom.debugflags=0x10		# Put boot2 into ZFS boot slot
    doit dd if=${dst}/boot/zfsboot of=/dev/${dev}s${s}a skip=1 seek=1024
    sysctl kern.geom.debugflags=0x0
}

boot_nogeli_mbr_zfs_uefi() {
    dev=$1
    dst=$2

    s=$(find-part $dev "!239")
    if [ -z "$s" ] ; then
	die "No ESP slice found"
    fi
    doit gpart bootcode -p ${efi2} -i ${s} ${dev}
}

boot_nogeli_mbr_zfs_both() {
    boot_nogeli_mbr_zfs_legacy $1 $2 $3
    boot_nogeli_mbr_zfs_uefi $1 $2 $3
}

boot_geli_gpt_ufs_legacy() {
    boot_nogeli_gpt_ufs_legacy $1 $2 $3
}

boot_geli_gpt_ufs_uefi() {
    boot_nogeli_gpt_ufs_uefi $1 $2 $3
}

boot_geli_gpt_ufs_both() {
    boot_nogeli_gpt_ufs_both $1 $2 $3
}

boot_geli_gpt_zfs_legacy() {
    boot_nogeli_gpt_zfs_legacy $1 $2 $3
}

boot_geli_gpt_zfs_uefi() {
    boot_nogeli_gpt_zfs_uefi $1 $2 $3
}

boot_geli_gpt_zfs_both() {
    boot_nogeli_gpt_zfs_both $1 $2 $3
}

# GELI+MBR is not a valid configuration
boot_geli_mbr_ufs_legacy() {
    exit 1
}

boot_geli_mbr_ufs_uefi() {
    exit 1
}

boot_geli_mbr_ufs_both() {
    exit 1
}

boot_geli_mbr_zfs_legacy() {
    exit 1
}

boot_geli_mbr_zfs_uefi() {
    exit 1
}

boot_geli_mbr_zfs_both() {
    exit 1
}

boot_nogeli_vtoc8_ufs_ofw() {
    dev=$1
    dst=$2

    # For non-native builds, ensure that geom_part(4) supports VTOC8.
    kldload geom_part_vtoc8.ko
    doit gpart bootcode -p ${vtoc8} ${dev}
}

DESTDIR=/

# Note: we really don't support geli boot in this script yet.
geli=nogeli

while getopts "b:d:f:g:o:s:" opt; do
    case "$opt" in
	b)
	    bios=${OPTARG}
	    ;;
	d)
	    DESTDIR=${OPTARG}
	    ;;
	f)
	    fs=${OPTARG}
	    ;;
	g)
	    case ${OPTARG} in
		[Yy][Ee][Ss]|geli) geli=geli ;;
		*) geli=nogeli ;;
	    esac
	    ;;
	o)
	    opts=${OPTARG}
	    ;;
	s)
	    scheme=${OPTARG}
	    ;;
    esac
done

shift $((OPTIND-1))
dev=$1

# For gpt, we need to install pmbr as the primary boot loader
# it knows about 
gpt0=${DESTDIR}/boot/pmbr
gpt2=${DESTDIR}/boot/gptboot
gptzfs2=${DESTDIR}/boot/gptzfsboot

# For gpt + EFI we install the ESP
# XXX This should use newfs or makefs, but it doesn't yet
efi1=${DESTDIR}/boot/boot1.efi
efi2=${DESTDIR}/boot/boot1.efifat

# For MBR, we have lots of choices, but select mbr, boot0 has issues with UEFI
mbr0=${DESTDIR}/boot/mbr
mbr2=${DESTDIR}/boot/boot

# VTOC8
vtoc8=${DESTDIR}/boot/boot1

# sanity check here

eval boot_${geli}_${scheme}_${fs}_${bios} $dev $DESTDIR $opts || echo "Unsupported boot env: ${geli}-${scheme}-${fs}-${bios}"
