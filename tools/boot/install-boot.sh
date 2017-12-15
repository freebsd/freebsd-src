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

find-part() {
    dev=$1
    part=$2

    gpart show $dev | tail +2 | awk '$4 == "'$part'" { print $3; }'
}

boot_nogeli_gpt_zfs_legacy() {
    dev=$1
    dst=$2

    idx=$(find-part $dev "freebsd-boot")
    if [ -z "$idx" ] ; then
	die "No freebsd-boot partition found"
    fi
    gpart bootcode -b ${gpt0} -p ${gpt2} -i $idx $dev
    exit 0
}

boot_nogeli_gpt_ufs_legacy() {
    dev=$1
    dst=$2

    idx=$(find-part $dev "freebsd-boot")
    if [ -z "$idx" ] ; then
	die "No freebsd-boot partition found"
    fi
    gpart bootcode -b ${gpt0} -p ${gpt2} -i $idx $dev
    exit 0
}

boot_nogeli_mbr_zfs_legacy() {
    dev=$1
    dst=$2

    # search to find the BSD slice
    s=$(findpart $dev "freebsd-zfs")
    if [ -z "$s" ] ; then
	die "No freebsd-zfs slice found"
    fi
    # search to find the freebsd-zfs partition within the slice
    # Or just assume it is 'a' because it has to be since it fails otherwise
    dd if=${dst}/boot/zfsboot of=/tmp/zfsboot1 count=1
    gpart bootcode -b /tmp/zfsboo1 ${dev}s${s}	# Put boot1 into the start of part
    sysctl kern.geom.debugflags=0x10		# Put boot2 into ZFS boot slot
    dd if=${dst}/boot/zfsboot of=/dev/${dev}s${s} iseek=1 seek=1024
    sysctl kern.geom.debugflags=0x0

    exit 0
}

boot_nogeli_mbr_ufs_legacy() {
    dev=$1
    dst=$2

    gpart bootcode -b ${mbr0} ${dev}
    s=$(findpart $dev "freebsd-ufs")
    if [ -z "$s" ] ; then
	die "No freebsd-zfs slice found"
    fi
    gpart bootcode -p ${mbr2} ${dev}s${s}
    exit 0
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

# For MBR, we have lots of choices, but select boot0
mbr0=${DESTDIR}/boot/boot0
mbr2=${DESTDIR}/boot/boot

# sanity check here

eval boot_${geli}_${scheme}_${fs}_${bios} $dev $DESTDIR $opts || echo "Unsupported boot env: ${geli}-${scheme}-${fs}-${bios}"
