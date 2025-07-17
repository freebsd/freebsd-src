#!/bin/sh

#
# Installs/updates the necessary boot blocks for the desired boot environment
#
# Lightly tested.. Intended to be installed, but until it matures, it will just
# be a boot tool for regression testing.

# insert code here to guess what you have -- yikes!

# Minimum size of FAT filesystems, in KB.
fat32min=33292
fat16min=2100

die() {
    echo $*
    exit 1
}

doit() {
    echo $*
    eval $*
}

find_part() {
    dev=$1
    part=$2

    gpart show $dev | tail +2 | awk '$4 == "'$part'" { print $3; }'
}

get_uefi_bootname() {

    case ${TARGET:-$(uname -m)} in
        amd64) echo bootx64 ;;
        arm64) echo bootaa64 ;;
        i386) echo bootia32 ;;
        arm) echo bootarm ;;
        riscv) echo bootriscv64 ;;
        *) die "machine type $(uname -m) doesn't support UEFI" ;;
    esac
}

make_esp_file() {
    local file sizekb device stagedir fatbits efibootname

    file=$1
    sizekb=$2

    if [ "$sizekb" -ge "$fat32min" ]; then
        fatbits=32
    elif [ "$sizekb" -ge "$fat16min" ]; then
        fatbits=16
    else
        fatbits=12
    fi

    stagedir=$(mktemp -d /tmp/stand-test.XXXXXX)
    mkdir -p "${stagedir}/EFI/BOOT"

    # Allow multiple files to be copied.
    # We do this in pairs, e.g:
    # make_esp_file ... loader1.efi bootx64 loader2.efi bootia32
    #
    # If the second argument is left out,
    # determine it automatically.
    shift; shift # Skip $file and $sizekb
    while [ ! -z $1 ]; do
        if [ ! -z $2 ]; then
            efibootname=$2
        else
            efibootname=$(get_uefi_bootname)
        fi
        cp "$1" "${stagedir}/EFI/BOOT/${efibootname}.efi"

        shift; shift || : # Ignore failure to shift
    done

    makefs -t msdos \
	-o fat_type=${fatbits} \
	-o sectors_per_cluster=1 \
	-o volume_label=EFISYS \
	-s ${sizekb}k \
	"${file}" "${stagedir}"
    rm -rf "${stagedir}"
}

make_esp_device() {
    local dev file dst mntpt fstype efibootname kbfree loadersize efibootfile
    local isboot1 existingbootentryloaderfile bootorder bootentry

    # ESP device node
    dev=$1
    file=$2
    # Allow caller to override the default
    if [ ! -z $3 ]; then
	    efibootname=$3
    else
	    efibootname=$(get_uefi_bootname)
    fi

    dst=$(basename ${file%.efi})
    mntpt=$(mktemp -d /tmp/stand-test.XXXXXX)

    # See if we're using an existing (formatted) ESP
    fstype=$(fstyp "${dev}")

    if [ "${fstype}" != "msdosfs" ]; then
        newfs_msdos -F 32 -c 1 -L EFISYS "${dev}" > /dev/null 2>&1
    fi

    mount -t msdosfs "${dev}" "${mntpt}"
    if [ $? -ne 0 ]; then
        die "Failed to mount ${dev} as an msdosfs filesystem"
    fi

    echo "Mounted ESP ${dev} on ${mntpt}"

    kbfree=$(df -k "${mntpt}" | tail -1 | cut -w -f 4)
    loadersize=$(stat -f %z "${file}")
    loadersize=$((loadersize / 1024))

    # Check if /EFI/BOOT/BOOTxx.EFI is the FreeBSD boot1.efi
    # If it is, remove it to avoid leaving stale files around
    efibootfile="${mntpt}/EFI/BOOT/${efibootname}.efi"
    if [ -f "${efibootfile}" ]; then
        isboot1=$(strings "${efibootfile}" | grep "FreeBSD EFI boot block")

        if [ -n "${isboot1}" ] && [ "$kbfree" -lt "${loadersize}" ]; then
            echo "Only ${kbfree}KB space remaining: removing old FreeBSD boot1.efi file /EFI/BOOT/${efibootname}.efi"
            rm "${efibootfile}"
            rmdir "${mntpt}/EFI/BOOT"
        else
            echo "${kbfree}KB space remaining on ESP: renaming old boot1.efi file /EFI/BOOT/${efibootname}.efi /EFI/BOOT/${efibootname}-old.efi"
            mv "${efibootfile}" "${mntpt}/EFI/BOOT/${efibootname}-old.efi"
        fi
    fi

    if [ ! -f "${mntpt}/EFI/freebsd/${dst}.efi" ] && [ "$kbfree" -lt "$loadersize" ]; then
        umount "${mntpt}"
	rmdir "${mntpt}"
        echo "Failed to update the EFI System Partition ${dev}"
        echo "Insufficient space remaining for ${file}"
        echo "Run e.g \"mount -t msdosfs ${dev} /mnt\" to inspect it for files that can be removed."
        die
    fi

    mkdir -p "${mntpt}/EFI/freebsd"

    # Keep a copy of the existing loader.efi in case there's a problem with the new one
    if [ -f "${mntpt}/EFI/freebsd/${dst}.efi" ] && [ "$kbfree" -gt "$((loadersize * 2))" ]; then
        cp "${mntpt}/EFI/freebsd/${dst}.efi" "${mntpt}/EFI/freebsd/${dst}-old.efi"
    fi

    echo "Copying loader to /EFI/freebsd on ESP"
    cp "${file}" "${mntpt}/EFI/freebsd/${dst}.efi"

    # efibootmgr won't work on systems with ia32 UEFI firmware
    # since we only use it to boot the 64-bit kernel
    if [ -n "${updatesystem}" ] && [ ${efibootname} != "bootia32" ]; then
        existingbootentryloaderfile=$(efibootmgr -v | grep "${mntpt}//EFI/freebsd/${dst}.efi")

        if [ -z "$existingbootentryloaderfile" ]; then
            # Try again without the double forward-slash in the path
            existingbootentryloaderfile=$(efibootmgr -v | grep "${mntpt}/EFI/freebsd/${dst}.efi")
        fi

        if [ -z "$existingbootentryloaderfile" ]; then
            echo "Creating UEFI boot entry for FreeBSD"
            efibootmgr --create --label FreeBSD --loader "${mntpt}/EFI/freebsd/${dst}.efi" > /dev/null
            if [ $? -ne 0 ]; then
                die "Failed to create new boot entry"
            fi

            # When creating new entries, efibootmgr doesn't mark them active, so we need to
            # do so. It doesn't make it easy to find which entry it just added, so rely on
            # the fact that it places the new entry first in BootOrder.
            bootorder=$(efivar --name 8be4df61-93ca-11d2-aa0d-00e098032b8c-BootOrder --print --no-name --hex | head -1)
            bootentry=$(echo "${bootorder}" | cut -w -f 3)$(echo "${bootorder}" | cut -w -f 2)
            echo "Marking UEFI boot entry ${bootentry} active"
            efibootmgr --activate "${bootentry}" > /dev/null
        else
            echo "Existing UEFI FreeBSD boot entry found: not creating a new one"
        fi
    else
	# Configure for booting from removable media
	if [ ! -d "${mntpt}/EFI/BOOT" ]; then
		mkdir -p "${mntpt}/EFI/BOOT"
	fi
	cp "${file}" "${mntpt}/EFI/BOOT/${efibootname}.efi"
    fi

    umount "${mntpt}"
    rmdir "${mntpt}"
    echo "Finished updating ESP"
}

make_esp() {
    local file loaderfile

    file=$1
    loaderfile=$2

    if [ -f "$file" ]; then
        make_esp_file ${file} ${fat32min} ${loaderfile}
    else
        make_esp_device ${file} ${loaderfile}
    fi
}

make_esp_mbr() {
    dev=$1
    dst=$2

    s=$(find_part $dev "!239")
    if [ -z "$s" ] ; then
	s=$(find_part $dev "efi")
	if [ -z "$s" ] ; then
	    die "No ESP slice found"
    	fi
    fi
    make_esp /dev/${dev}s${s} ${dst}/boot/loader.efi
}

make_esp_gpt() {
    dev=$1
    dst=$2

    idx=$(find_part $dev "efi")
    if [ -z "$idx" ] ; then
	die "No ESP partition found"
    fi
    make_esp /dev/${dev}p${idx} ${dst}/boot/loader.efi
}

boot_nogeli_gpt_ufs_legacy() {
    dev=$1
    dst=$2

    idx=$(find_part $dev "freebsd-boot")
    if [ -z "$idx" ] ; then
	die "No freebsd-boot partition found"
    fi
    doit gpart bootcode -b ${gpt0} -p ${gpt2} -i $idx $dev
}

boot_nogeli_gpt_ufs_uefi() {
    make_esp_gpt $1 $2
}

boot_nogeli_gpt_ufs_both() {
    boot_nogeli_gpt_ufs_legacy $1 $2 $3
    boot_nogeli_gpt_ufs_uefi $1 $2 $3
}

boot_nogeli_gpt_zfs_legacy() {
    dev=$1
    dst=$2

    idx=$(find_part $dev "freebsd-boot")
    if [ -z "$idx" ] ; then
	die "No freebsd-boot partition found"
    fi
    doit gpart bootcode -b ${gpt0} -p ${gptzfs2} -i $idx $dev
}

boot_nogeli_gpt_zfs_uefi() {
    make_esp_gpt $1 $2
}

boot_nogeli_gpt_zfs_both() {
    boot_nogeli_gpt_zfs_legacy $1 $2 $3
    boot_nogeli_gpt_zfs_uefi $1 $2 $3
}

boot_nogeli_mbr_ufs_legacy() {
    dev=$1
    dst=$2

    doit gpart bootcode -b ${mbr0} ${dev}
    s=$(find_part $dev "freebsd")
    if [ -z "$s" ] ; then
	die "No freebsd slice found"
    fi
    doit gpart bootcode -p ${mbr2} ${dev}s${s}
}

boot_nogeli_mbr_ufs_uefi() {
    make_esp_mbr $1 $2
}

boot_nogeli_mbr_ufs_both() {
    boot_nogeli_mbr_ufs_legacy $1 $2 $3
    boot_nogeli_mbr_ufs_uefi $1 $2 $3
}

boot_nogeli_mbr_zfs_legacy() {
    dev=$1
    dst=$2

    # search to find the BSD slice
    s=$(find_part $dev "freebsd")
    if [ -z "$s" ] ; then
	die "No BSD slice found"
    fi
    idx=$(find_part ${dev}s${s} "freebsd-zfs")
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
    make_esp_mbr $1 $2
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

usage() {
	printf 'Usage: %s -b bios [-d destdir] -f fs [-g geli] [-h] [-o optargs] -s scheme <bootdev>\n' "$0"
	printf 'Options:\n'
	printf ' bootdev       device to install the boot code on\n'
	printf ' -b bios       bios type: legacy, uefi or both\n'
	printf ' -d destdir    destination filesystem root\n'
	printf ' -f fs         filesystem type: ufs or zfs\n'
	printf ' -g geli       yes or no\n'
	printf ' -h            this help/usage text\n'
	printf ' -u            Run commands such as efibootmgr to update the\n'
	printf '               currently running system\n'
	printf ' -o optargs    optional arguments\n'
	printf ' -s scheme     mbr or gpt\n'
	exit 0
}

srcroot=/

# Note: we really don't support geli boot in this script yet.
geli=nogeli

while getopts "b:d:f:g:ho:s:u" opt; do
    case "$opt" in
	b)
	    bios=${OPTARG}
	    ;;
	d)
	    srcroot=${OPTARG}
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
	u)
	    updatesystem=1
	    ;;
	o)
	    opts=${OPTARG}
	    ;;
	s)
	    scheme=${OPTARG}
	    ;;

	?|h)
            usage
            ;;
    esac
done

if [ -n "${scheme}" ] && [ -n "${fs}" ] && [ -n "${bios}" ]; then
    shift $((OPTIND-1))
    dev=$1
fi

# For gpt, we need to install pmbr as the primary boot loader
# it knows about 
gpt0=${srcroot}/boot/pmbr
gpt2=${srcroot}/boot/gptboot
gptzfs2=${srcroot}/boot/gptzfsboot

# For MBR, we have lots of choices, but select mbr, boot0 has issues with UEFI
mbr0=${srcroot}/boot/mbr
mbr2=${srcroot}/boot/boot

# sanity check here

# Check if we've been given arguments. If not, this script is probably being
# sourced, so we shouldn't run anything.
if [ -n "${dev}" ]; then
	eval boot_${geli}_${scheme}_${fs}_${bios} $dev $srcroot $opts || echo "Unsupported boot env: ${geli}-${scheme}-${fs}-${bios}"
fi
