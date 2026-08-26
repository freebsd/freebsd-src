#!/bin/sh
#
# boot-test.sh - Automated boot loader regression tests
#
# Builds a minimal bootable tree, assembles disk images for all supported boot
# configurations, then runs each in QEMU with a timeout looking for "SUCCESS".
#
# All tests run as an unprivileged user. No root required, with one exception:
# netboot-bios/netboot-efi need a real tap(4) device + dnsmasq (so DHCP can
# carry a root-path) instead of QEMU's slirp networking, and both creating a tap
# and giving it an address require root. That setup runs once (via sudo,
# prompting interactively) and is left running -- later runs detect the existing
# setup and skip sudo entirely.  Assumes buildworld and buildkernel have already
# been done for the target architecture.
#
# Usage:
#   cd /usr/src/stand && sh ../tools/boot/boot-test.sh [options]
#
# Options:
#   -a ARCH	Architecture to test; repeat -a to test several
#		(default: host arch)
#		Supported: amd64, aarch64, armv7, riscv64, powerpc, powerpc64,
#		powerpc64le
#   -A		Test every supported architecture
#   -b		Skip build/install phase (reuse existing tree)
#   -B		Skip build/install and image creation (reuse existing images)
#   -l          Tell us the log directory and exit
#   -j JOBS	Max parallel QEMU instances (default: unlimited)
#   -o DIR	Output directory for images and logs
#   -t REGEX	Only run tests matching REGEX
#   -T SECONDS	QEMU timeout (default: per-arch, 60; 180 for powerpc64)
#   --netboot-teardown
#		Destroy the tap(4)/dnsmasq netboot setup (run with sudo)

set -e

die() {
    echo "FATAL: $*" >&2
    exit 1
}

# FreeBSD port package (origin) that provides a command or file; "" = base
# system.
pkg_for() {
    case "$1" in
    jq)			echo textproc/jq ;;
    expect)		echo lang/expect ;;
    qemu-system-*)	echo emulators/qemu ;;
    *ipxe*)		echo sysutils/ipxe ;;
    *syslinux*|*memdisk*) echo sysutils/syslinux ;;
    *edk2*)		echo emulators/qemu ;;	# edk2-*.fd ship with qemu
    *)			echo "" ;;		# makefs/mkimg etc. = base
    esac
}

# Hard requirement: a command that must be in PATH, else abort naming the pkg.
need_cmd() {
    which "$1" >/dev/null 2>&1 && return 0
    p=$(pkg_for "$1")
    [ -n "${p}" ] && die "$1 not found; install with: pkg install ${p##*/}"
    die "$1 not found (expected in the base system)"
}

# Optional requirement: a file for a feature; warn + skip (return 1) if absent.
have_file() {
    [ -e "$1" ] && return 0
    p=$(pkg_for "$1")
    echo "  WARNING: $1 missing${p:+; pkg install ${p##*/}} -- skipping" >&2
    return 1
}

# --------------------------------------------------------------------------
# Architecture configuration
#
# Per-arch parameters live in boot-test.json, as an arch object merged over a
# "defaults" object. Arrays are expanded to a space separated list. Missing
# values default to an empty string. ARCH_CAPS is a cached copy of the
# capabilities array for the architecture. TARGET and TARGET_ARCH are cached due
# to heavy use. Other parameters are fetched as needed as their use is
# infrequent.
# --------------------------------------------------------------------------

# Test whether the current arch declares a capability, e.g. "has efi".
has() {
    case " ${ARCH_CAPS} " in
	*" $1 "*) return 0 ;;
    esac
    return 1
}

# Fetch the named parameter for this $ARCH.
param() {
    jq -r --arg a "${ARCH}" --arg k "$1" '
	(.defaults + .arch[$a])[$k] as $v
	| if   ($v | type) == "array" then $v | join(" ")
	  elif $v == null             then ""
	  else                             $v end
    ' "${CONF}"
}

# Validate ${ARCH} and cache the parameters that are heavily used in global
# variables.
load_arch_config() {
    jq -e --arg a "${ARCH}" '.arch | has($a)' "${CONF}" >/dev/null 2>&1 \
	|| die "Unknown architecture: ${ARCH}"
    ARCH_CAPS=$(param caps)
    TARGET=$(param target)
    TARGET_ARCH=$(param target_arch)
}

# Derive every per-arch value for ${1} into the ARCH_*/TARGET/OUTDIR/... globals.
# Callers isolate architectures via subshells (build) or job backgrounding (run)
# -- both snapshot these globals -- so they never collide across arches, and we
# never have to pass the whole bundle around.
setup_arch_env() {
    ARCH=$1
    load_arch_config
    TIMEOUT=${TIMEOUT_OVERRIDE:-$(param timeout)}
    MK="make TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH}"
    ARCH_OBJDIR=$(${MK} -v .OBJDIR)
    [ -n "${ARCH_OBJDIR}" ] || die "Cannot determine OBJDIR for ${ARCH}"
    OUTDIR=${OUTDIR_OVERRIDE:-${ARCH_OBJDIR}/boot-test}
    IMGDIR=${OUTDIR}/images
    LOGDIR=${OUTDIR}/logs
    DESTDIR=${OUTDIR}/tree
    TESTLIST=${OUTDIR}/test-list.txt
    mkdir -p ${IMGDIR} ${LOGDIR}
}

# --- Configuration ---

SKIP_BUILD=false
SKIP_IMAGES=false
TEST_FILTER=""
MAX_JOBS=0
OUTDIR_OVERRIDE=""	# from -o; only valid with a single arch
TIMEOUT_OVERRIDE=""	# from -T; else each arch's param timeout
ARCHES=""		# from -a (repeatable) / -A; defaults to $(uname -p)
ALL=false

# State shared by tap(4)/dnsmasq netboot networking (see netboot_network_setup /
# netboot_helper below). Key off user's ID to allow multiple people to
# run the script at the same time.
NETBOOT_STATE_DIR=${TMPDIR:-/tmp}/boot-test-net.${SUDO_UID:-$(id -u)}

# --netboot-helper/--netboot-teardown are internal entry points used to run
# privileged setup/teardown via sudo (see netboot_network_setup); they bypass
# normal option parsing and are dispatched once every function is defined, in
# the Main section at the bottom of this script.
NETBOOT_MODE=""
case "$1" in
    --netboot-helper)   NETBOOT_MODE=helper; NETBOOT_HELPER_PLAN=$2 ;;
    --netboot-teardown) NETBOOT_MODE=teardown ;;
esac

do_report_dirs=false
if [ -z "${NETBOOT_MODE}" ]; then
    while getopts "a:AbBlj:o:t:T:" opt; do
	case "$opt" in
	    a) ARCHES="${ARCHES} $OPTARG" ;;
	    A) ALL=true ;;
	    b) SKIP_BUILD=true ;;
	    B) SKIP_BUILD=true; SKIP_IMAGES=true ;;
	    j) MAX_JOBS="$OPTARG" ;;
	    l) do_report_dirs=true ;;
	    o) OUTDIR_OVERRIDE="$OPTARG" ;;
	    t) TEST_FILTER="$OPTARG" ;;
	    T) TIMEOUT_OVERRIDE="$OPTARG" ;;
	    ?) echo "Usage: $0 [-a arch]... | -A] [-b] [-B] [-t regex] [-T secs] [-j jobs] [-o dir]" >&2
	       exit 1 ;;
	esac
    done

    # Resolve the config file next to this script before we cd elsewhere.
    CONF="$(cd "$(dirname "$0")" && pwd)/boot-test.json"
    [ -f "${CONF}" ] || die "Config file not found: ${CONF}"

    SRCTOP=$(make -v SRCTOP) || die "Run from stand/ directory in a FreeBSD source tree"
    cd ${SRCTOP}/stand

    # Build the architecture list from json. Partially supported architectures
    # are omitted from -A, but accessible with a direct -a.
    if ${ALL}; then
	for a in $(jq -r '.arch | keys_unsorted[]' "${CONF}"); do
	    if [ $(jq ".arch.${a}.disabled" "${CONF}") != "true" ]; then
		ARCHES="$ARCHES $a"
	    fi
	done
    fi
    [ -n "${ARCHES}" ] || ARCHES=$(uname -p)

    # -o names one output directory, so it only makes sense for a single arch.
    if [ -n "${OUTDIR_OVERRIDE}" ] && [ $(echo ${ARCHES} | wc -w) -gt 1 ]; then
	die "-o cannot be combined with multiple architectures"
    fi
fi

# The smallest FAT32 filesystem is 33292 KB
espsize=33292

# Linux kernel version for linuxboot tests
LINUX_VERSION=6.18.2

# --------------------------------------------------------------------------
# QEMU command builders
#
# qemu_base wraps the constant bits -- binary, memory, machine, per-arch extra
# flags, and the -nographic/serial tail -- around the device arguments each
# specific builder passes in (disks, CDs, firmware, bios).
# --------------------------------------------------------------------------

qemu_base() {
    echo "$(param qemu_bin) -m 1g $(param qemu_machine) $(param qemu_extra) $* -nographic -monitor none -serial stdio"
}

# -drive for the EFI firmware pflash; empty when the arch has no separate
# firmware (e.g. riscv64's u-boot payload).
qemu_efi_firmware() {
    [ -z "$(param efi_firmware)" ] && return 0
    echo "-drive file=$(param efi_firmware),format=raw,if=pflash,readonly=on"
}

# Custom OpenBIOS cached beside the ISOs, if present (QEMU's bundled one is
# missing fixes we need for now); empty otherwise.
qemu_ofw_bios() {
    [ -f "${ISODIR}/openbios-ppc" ] || return 0
    echo "-bios ${ISODIR}/openbios-ppc"
}

qemu_bios()       { qemu_base "-drive file=$1,format=raw"; }
qemu_efi()        { qemu_base "$(qemu_efi_firmware) -drive file=$1,format=raw"; }
qemu_bios_cdrom() { qemu_base "-cdrom $1"; }
qemu_efi_cdrom()  { qemu_base "$(qemu_efi_firmware) -cdrom $1"; }

# OFW disk on the default (macio IDE) bus: OpenBIOS aliases it "hd" and
# auto-probes hd:,\\:tbxi for the Apple_Bootstrap (boot1.hfs) partition.
# virtio disks are not reachable as "hd", so OF finds nothing and drops to "0 >".
qemu_ofw()        { qemu_base "$(qemu_ofw_bios) -drive file=$1,format=raw"; }
# -boot d boots the (macio IDE) CD-ROM, which OpenBIOS probes cd:,\\:tbxi.
qemu_ofw_cdrom()  { qemu_base "$(qemu_ofw_bios) -boot d -cdrom $1"; }

# pseries PReP disk boot: virtio-blk (vtbd0), matching freebsd-ci; SLOF finds
# the PReP boot partition on it and runs boot1.elf.
qemu_prep()       { qemu_base "-drive if=none,file=$1,format=raw,id=hd0 -device virtio-blk,drive=hd0"; }
# pseries CD boot: SLOF boots the El Torito CHRP image; -boot d selects the CD.
qemu_prep_cdrom() { qemu_base "-cdrom $1 -boot d"; }

# Direct linuxboot: hand the Linux kernel and initrd to QEMU on the command
# line (-kernel/-initrd) rather than off an ESP.  Used by platforms with no
# EFI/ESP (e.g. powerpc64le/pseries); the FreeBSD root disk is attached the
# same way as every other test.
qemu_linuxboot() {
    extra="-kernel $2 -initrd $3"
    [ -n "$(param linux_console)" ] && extra="${extra} -append $(param linux_console)"
    qemu_base "${extra} -drive file=$1,format=raw"
}

# Netboot over $1 -- vmnet(4): dnsmasq owns DHCP/TFTP/root-path, which we use so
# we use tftp, not NFS, for all the files. QEMU's netdev type is still "tap" (it
# treats vmnet(4) and tap(4) identically).  $2 = bios|efi (efi adds the pflash
# firmware; bios relies on the NIC's PXE option ROM).
qemu_netboot() {
    netif=$1
    fw=""
    [ "$2" = efi ] && fw="$(qemu_efi_firmware)"
    qemu_base "${fw} -netdev tap,id=net0,ifname=${netif},script=no,downscript=no -device $(param netboot_nic),netdev=net0 -boot n"
}

# RAM-disk netboot (x86 EFI).  Boots iPXE from -hda (edk2's own PXE is disabled
# via fw_cfg); iPXE DHCPs, fetches the bootfile (an iPXE script) over TFTP, and
# chains loader.efi with memdisk=<url>.  The loader downloads that image and
# boots it entirely from RAM -- no NFS and no DHCP root-path needed.  Mirrors
# ~/memdisk/do-memddisk-efi.  ${OUTDIR}/netboot-vars.fd is a writable edk2 vars
# copy made by assemble_netboot.
#
# We'll need to to http boots in the future, and that will likely require
# we don't use the ipxe USB path we use here. We use that because Tianocore
# expects http/https booting when the obvious '-boot n' sort of things
# are used.
qemu_netboot_ramdisk() {
    netif=$1
    echo "$(param qemu_bin) -M q35 -cpu max -m 2g \
	-drive if=pflash,format=raw,readonly=on,file=$(param efi_firmware) \
	-drive if=pflash,format=raw,file=${OUTDIR}/netboot-vars.fd \
	-hda ${OUTDIR}/netboot-ipxe.img \
	-device virtio-net,netdev=net0 \
	-netdev tap,id=net0,ifname=${netif},script=no,downscript=no \
	-fw_cfg name=opt/org.tianocore/IPv4PXESupport,string=no \
	-fw_cfg name=opt/org.tianocore/IPv6PXESupport,string=no \
	-nographic -monitor none -serial stdio"
}

# --------------------------------------------------------------------------
# Phase 0: Extract minimal userland from release ISO
# --------------------------------------------------------------------------

# Binaries needed for a minimal bootable userland.
# Libraries are inferred from these via ldd on the host equivalents.
USERLAND_BINS="sbin/fastboot sbin/halt sbin/init bin/sh sbin/sysctl"

ISODIR=${HOME}/iso

find_iso() {
    local isotgt=${TARGET}
    [ ${TARGET} != ${TARGET_ARCH} ] && isotgt="${isotgt}-${TARGET_ARCH}"
    local isoname="${ISODIR}/FreeBSD-$(param freebsd_version)-RELEASE-${isotgt}-disc1.iso.xz"
    [ -f "${isoname}" ] && echo "${isoname}" && return
    die "No ISO found for ${ARCH}: ${isoname} not found"
}

install_minimal_userland() {
    # Split the declaration from the assignment: `local iso=$(find_iso)` would
    # swallow find_iso's exit status (local returns 0), so its die() -- which
    # runs in the $() subshell -- would not stop this script.
    local iso
    iso=$(find_iso) || exit 1
    echo "  Extracting minimal userland from release ISO ${iso}..."

    # Determine library paths from host equivalents of our binaries
    local host_bins=""
    for b in ${USERLAND_BINS}; do
	host_bins="${host_bins} /${b}"
    done
    lib_paths=$(ldd ${host_bins} 2>/dev/null \
	| awk 'NF == 4 { print $3 }' | sort -u)

    # Build the list of paths to extract: binaries + libraries + rtld
    local extract_list=""
    for b in ${USERLAND_BINS}; do
	extract_list="${extract_list} ./${b}"
    done
    for l in ${lib_paths}; do
	extract_list="${extract_list} .${l}"
    done
    extract_list="${extract_list} ./libexec/ld-elf.so.1"

    # Some architectures (e.g., armv7) need libgcc_s.so.1 even though the host
    # binaries don't. Always try to extract it. We'll ignore it if not there.
    extract_list="${extract_list} ./lib/libgcc_s.so.1"

    # Extract the files from the tarball.
    tar -C ${DESTDIR} -xf ${iso} ${extract_list} \
	>> ${LOGDIR}/installuserland.log 2>&1 || true
}

# --------------------------------------------------------------------------
# Phase 1: Build the boot tree
# --------------------------------------------------------------------------

build_tree() {
    echo "=== Phase 1: Building boot tree ==="

    rm -rf ${DESTDIR}
    mkdir -p ${DESTDIR}/boot/defaults
    mkdir -p ${DESTDIR}/boot/kernel
    mkdir -p ${DESTDIR}/boot/uboot
    mkdir -p ${DESTDIR}/sbin ${DESTDIR}/bin \
	${DESTDIR}/lib ${DESTDIR}/libexec \
	${DESTDIR}/etc ${DESTDIR}/dev

    # Install kernel
    # I'd prefer this to be MINIMAL, but GENERIC is needed until I work out what
    # different devices we boot from...
    (cd ${SRCTOP} && ${MK} installkernel \
	KERNCONF=$(param kernconf) \
	MODULES_OVERRIDE="ufs zfs acl_nfs4 crypto zlib cd9660" \
	DESTDIR=${DESTDIR} \
	MK_KERNEL_SYMBOLS=no \
	MK_INSTALL_AS_USER=yes) > ${LOGDIR}/installkernel.log 2>&1 \
	|| die "Kernel install failed (see ${LOGDIR}/installkernel.log)"

    # Install boot loaders
    ${MK} buildenv \
	DESTDIR=${DESTDIR} \
	MK_MAN=no \
	MK_INSTALL_AS_USER=yes \
	MK_DEBUG_FILES=no \
	BUILDENV_SHELL="make all install" \
	>> ${LOGDIR}/installloader.log 2>&1 \
	|| die "Boot loader install failed (see ${LOGDIR}/installloader.log)"

    # Install minimal userland (works for both native and cross builds)
    install_minimal_userland

    # Remove default loader symlinks -- we add them back per-image via
    # mtree overlays to test each loader variant individually.
    # /boot/loader is the BIOS stage-3 (amd64 only).
    # /boot/loader.efi is what boot1.efi chainloads (all EFI platforms).
    # OFW/PReP are the exception: their /boot/loader is the one real loader
    # (no lua/4th/simp variants are built), chainloaded by boot1.hfs (mac99)
    # or boot1.elf (pseries PReP), so keep it in the tree.
    has ofw || has prep || rm -f ${DESTDIR}/boot/loader
    rm -f ${DESTDIR}/boot/loader.efi

    # Serial console configuration.  boot.config is consumed only by the
    # BIOS boot blocks; its -h/-D/-S flags are meaningless on EFI/OFW, so
    # only write it where BIOS booting is supported.
    if has bios; then
	echo -h -D -S115200 > ${DESTDIR}/boot.config
    fi
    # Unified loader.conf: always load ufs, zfs, and cd9660
    cat > ${DESTDIR}/boot/loader.conf <<EOF
boot_serial=YES
comconsole_speed=115200
autoboot_delay=1
ufs_load="YES"
zfs_load="YES"
cd9660_load="YES"
EOF
    local hints=$(param hints)
    if [ -n "${hints}" ] && [ -f "${SRCTOP}/${hints}" ]; then
	cp "${SRCTOP}/${hints}" ${DESTDIR}/boot/device.hints
    fi

    # Test /etc/rc - prints success and halts
    cat > ${DESTDIR}/etc/rc <<'RCEOF'
#!/bin/sh

sysctl machdep.bootmethod
echo "RC COMMAND RUNNING -- SUCCESS!!!!!"
halt -p
RCEOF
    chmod +x ${DESTDIR}/etc/rc

    # Create fstab used by UFS mtree overlays
    cat > ${OUTDIR}/fstab.ufs <<EOF
/dev/ufs/root	/	ufs	rw	1	1
EOF
    # Create fstab used by CD mtree overlays
    cat > ${OUTDIR}/fstab.cd <<EOF
/dev/iso9660/FBSDTEST	/	cd9660	ro	0	0
EOF

    echo "Boot tree built in ${DESTDIR}"
}

# --------------------------------------------------------------------------
# Phase 2: Create base filesystem images
#
# Uses mtree overlays to vary the /boot/loader and /etc/fstab
# without copying the tree. The base tree has no /boot/loader or
# /etc/fstab; each image adds what it needs via an mtree spec file
# passed as a second source to makefs.
# --------------------------------------------------------------------------

# Create a UFS image with a specific loader variant via mtree overlay
make_one_ufs() {
    variant=$1
    img=${IMGDIR}/bootable-ufs-${variant}.img
    mt=$(mktemp ${OUTDIR}/ufs-mtree.XXXXXX)

    echo "  Creating UFS image with loader_${variant}..."
    echo "./etc/fstab type=file mode=0644 contents=${OUTDIR}/fstab.ufs" > ${mt}
    # BIOS: /boot/loader -> loader_<variant>
    if [ -n "$(param bios_loaders)" ]; then
	echo "./boot/loader type=file mode=0644 contents=${DESTDIR}/boot/loader_${variant}" >> ${mt}
    fi
    # OFW: boot1.hfs chainloads the single /boot/loader already in the
    # tree (no per-variant overlay needed).
    # EFI: /boot/loader.efi -> loader_<variant>.efi (needed for boot1.efi chainload)
    if has efi; then
	echo "./boot/loader.efi type=file mode=0755 contents=${DESTDIR}/boot/loader_${variant}.efi" >> ${mt}
    fi
    makefs -t ffs -B $(param byte_order) -M 10m -o label=root -o version=2 \
	${img} ${mt} ${DESTDIR} >> ${LOGDIR}/imagebuild.log 2>&1
    rm -f ${mt}
}

# Create a ZFS image with a specific loader variant via mtree overlay
make_one_zfs() {
    variant=$1
    img=${IMGDIR}/bootable-zfs-${variant}.img
    mt=$(mktemp ${OUTDIR}/zfs-mtree.XXXXXX)

    echo "  Creating ZFS image with loader_${variant}..."
    > ${mt}
    # BIOS: /boot/loader -> loader_<variant>
    if [ -n "$(param bios_loaders)" ]; then
	echo "./boot/loader type=link link=loader_${variant}" >> ${mt}
    fi
    # OFW: boot1.hfs chainloads the single /boot/loader already in the
    # tree (no per-variant overlay needed).
    # EFI: /boot/loader.efi -> loader_<variant>.efi (needed for boot1.efi chainload)
    if has efi; then
	echo "./boot/loader.efi type=file mode=0755 contents=${DESTDIR}/boot/loader_${variant}.efi" >> ${mt}
    fi
    makefs -t zfs -s 100m \
	-o poolname=ztestroot -o bootfs=ztestroot -o rootpath=/ \
	${img} ${mt} ${DESTDIR} >> ${LOGDIR}/imagebuild.log 2>&1
    rm -f ${mt}
}

# Create an ESP with the given EFI loader using an mtree spec
make_one_esp() {
    loader_name=$1
    esp=${IMGDIR}/${loader_name}.esp
    mt=$(mktemp ${OUTDIR}/esp-mtree.XXXXXX)

    echo "  Creating ESP with ${loader_name}.efi..."
    cat > ${mt} <<EOF
./efi type=dir uname=root gname=wheel mode=0755
./efi/boot type=dir uname=root gname=wheel mode=0755
./efi/boot/$(param efi_bootname).efi type=file uname=root gname=wheel mode=0755 contents=${DESTDIR}/boot/${loader_name}.efi
EOF
    makefs -t msdos \
	-o fat_type=32 \
	-o sectors_per_cluster=1 \
	-o volume_label=EFISYS \
	-s ${espsize}k \
	${esp} ${mt} >> ${LOGDIR}/imagebuild.log 2>&1
    rm -f ${mt}
}

# Create a small ESP for CD hybrid boot using an mtree spec
make_cd_esp() {
    file=$1
    loader=$2
    mt=$(mktemp ${OUTDIR}/cd-esp-mtree.XXXXXX)

    cat > ${mt} <<EOF
./efi type=dir uname=root gname=wheel mode=0755
./efi/boot type=dir uname=root gname=wheel mode=0755
./efi/boot/$(param efi_bootname).efi type=file uname=root gname=wheel mode=0755 contents=${loader}
EOF
    makefs -t msdos \
	-o fat_type=12 \
	-o sectors_per_cluster=1 \
	-o volume_label=EFISYS \
	-s 2048k \
	${file} ${mt} >> ${LOGDIR}/imagebuild.log 2>&1
    rm -f ${mt}
}

# Find the pre-built Linux kernel EFI binary for linuxboot
find_linux_kernel() {
    target_arch=$(${MK} -v TARGET_ARCH)
    linuxboot_dir=${ARCH_OBJDIR}/../../linuxboot/data/output

    # amd64 has .efi suffix, others don't
    for f in \
	${linuxboot_dir}/${target_arch}.linux.v${LINUX_VERSION}.efi \
	${linuxboot_dir}/${target_arch}.linux.v${LINUX_VERSION} \
	${linuxboot_dir}/${target_arch}.v${LINUX_VERSION}.efi \
	${linuxboot_dir}/${target_arch}.v${LINUX_VERSION}; do
	if [ -f "$f" ]; then
	    echo "$f"
	    return
	fi
    done
    return 1
}

# Build a linuxboot initrd using tar --format newc with an mtree spec.
# No root/sudo required -- device nodes are written directly into the
# cpio archive via mtree type=char entries.
make_linuxboot_initrd() {
    initrd=${IMGDIR}/linuxboot-initrd.cpio.gz
    mt=$(mktemp ${OUTDIR}/initrd-mtree.XXXXXX)

    echo "  Creating linuxboot initrd..."

    # Build mtree spec for the initrd contents
    cat > ${mt} <<EOF
./init type=file mode=0755 contents=${DESTDIR}/boot/loader.kboot
./dev type=dir mode=0755
./dev/console type=char mode=0600 device=freebsd,5,0
./dev/tty type=char mode=0600 device=freebsd,5,1
./dev/ttyS0 type=char mode=0600 device=freebsd,4,64
./boot type=dir mode=0755
./boot/defaults type=dir mode=0755
./boot/defaults/loader.conf type=file mode=0644 contents=${DESTDIR}/boot/defaults/loader.conf
./boot/lua type=dir mode=0755
EOF

    # Add all lua files
    for f in ${DESTDIR}/boot/lua/*.lua; do
	[ -f "$f" ] || continue
	bn=$(basename $f)
	echo "./boot/lua/${bn} type=file mode=0644 contents=$f" >> ${mt}
    done

    # Add loader.help.kboot if present
    if [ -f "${DESTDIR}/boot/loader.help.kboot" ]; then
	echo "./boot/loader.help.kboot type=file mode=0644 contents=${DESTDIR}/boot/loader.help.kboot" >> ${mt}
    fi

    # Create the kboot-specific loader.conf
    kboot_conf=$(mktemp ${OUTDIR}/kboot-loader-conf.XXXXXX)
    cat > ${kboot_conf} <<EOF
# Kboot configuration -- FreeBSD ${ARCH}
boot_serial="YES"
EOF
    if [ "${ARCH}" = "amd64" ]; then
	cat >> ${kboot_conf} <<EOF
hw.uart.console="io:1016,br:115200"
EOF
    fi
    echo "./boot/loader.conf type=file mode=0644 contents=${kboot_conf}" >> ${mt}

    # Create the initrd as a gzip-compressed newc cpio archive
    tar --format newc -cf - @${mt} 2>> ${LOGDIR}/imagebuild.log | \
	gzip > ${initrd}

    rm -f ${mt} ${kboot_conf}
}

# Build a linuxboot ESP containing the Linux kernel, initrd, and startup.nsh
make_linuxboot_esp() {
    linux_kernel=$1
    esp=${IMGDIR}/linuxboot.esp
    mt=$(mktemp ${OUTDIR}/linuxboot-esp-mtree.XXXXXX)

    echo "  Creating linuxboot ESP..."

    # Generate startup.nsh
    startup=${OUTDIR}/startup.nsh
    cat > ${startup} <<EOF
\\linux.efi $(param linux_console) initrd=\\initrd
EOF

    cat > ${mt} <<EOF
./startup.nsh type=file mode=0644 contents=${startup}
./linux.efi type=file mode=0755 contents=${linux_kernel}
./initrd type=file mode=0644 contents=${IMGDIR}/linuxboot-initrd.cpio.gz
EOF
    makefs -t msdos \
	-o fat_type=32 \
	-o sectors_per_cluster=1 \
	-o volume_label=EFISYS \
	-s 100m \
	${esp} ${mt} >> ${LOGDIR}/imagebuild.log 2>&1
    rm -f ${mt}
}

make_base_images() {
    echo "=== Phase 2: Creating base filesystem images ==="

    # UFS images - one per BIOS loader variant, plus one for EFI (uses lua)
    if [ -n "$(param bios_loaders)" ]; then
	for v in $(param bios_loaders); do
	    make_one_ufs $v
	done
    else
	# EFI-only arches still need one UFS image
	make_one_ufs lua
    fi

    # ZFS images (if supported)
    if has zfs; then
	if [ -n "$(param bios_loaders)" ]; then
	    for v in $(param bios_loaders); do
		make_one_zfs $v
	    done
	else
	    make_one_zfs lua
	fi
    fi

    # ESP images (if EFI is supported)
    if has efi; then
	for l in $(param efi_loaders); do
	    make_one_esp $l
	done
    fi

    # Linuxboot initrd + ESP (if supported and Linux kernel is available)
    if has linuxboot; then
	linux_kernel=$(find_linux_kernel) || true
	if [ -n "${linux_kernel}" ]; then
	    make_linuxboot_initrd
	    # EFI arches chainload the kernel+initrd off an ESP; platforms
	    # without EFI hand them to QEMU directly (-kernel/-initrd), so
	    # they need no ESP.
	    has efi && make_linuxboot_esp ${linux_kernel}
	else
	    echo "  WARNING: Linux kernel not found for linuxboot, skipping"
	    echo "    Expected in: ${ARCH_OBJDIR}/../../linuxboot/data/output/"
	fi
    fi

    echo "Base images created in ${IMGDIR}"
}

# --------------------------------------------------------------------------
# Phase 3: Assemble disk images and register tests
# --------------------------------------------------------------------------

# Test registration - uses temp files since /bin/sh doesn't have arrays.
# ${TESTLIST} and ${OUTDIR} are set per-arch by setup_arch_env; the list is
# truncated in build_all before each arch's images are (re)assembled.
register_test() {
    name=$1
    shift
    echo "$*" > ${OUTDIR}/test-cmd-${name}.sh
    echo "${name}" >> ${TESTLIST}
}

# Like register_test, but for tests that need a real tap(4) interface
# (assigned later by netboot_network_setup, once every arch is built) instead
# of QEMU's slirp net.  The command is written out now with a placeholder tap
# name; netboot_network_setup patches it in once the tap is assigned.
# ${NETBOOT_PLAN} accumulates across every arch (unlike ${TESTLIST}, it is not
# truncated per-arch), so it must already exist by the time build_all runs --
# see Main.  $5 optionally names a different command builder than the default
# qemu_netboot (e.g. qemu_netboot_ramdisk) -- it's always called as
# "builder __NETBOOT_TAP__ ${fw}", so a non-default builder that doesn't need
# ${fw} just ignores its second arg.
register_netboot_test() {
    name=$1
    tftpdir=$2
    bootfile=$3
    fw=$4
    builder=${5:-qemu_netboot}
    cmdfile=${OUTDIR}/test-cmd-${name}.sh
    echo "$(${builder} __NETBOOT_TAP__ ${fw})" > ${cmdfile}
    echo "${name}" >> ${TESTLIST}
    echo "${cmdfile} ${tftpdir} ${bootfile}" >> ${NETBOOT_PLAN}
}

assemble_efi_gpt() {
    echo "  Assembling EFI+GPT images..."
    for loader in $(param efi_loaders); do
	esp=${IMGDIR}/${loader}.esp
	fstypes="ufs"
	has zfs && fstypes="ufs zfs"
	for fs in ${fstypes}; do
	    name="efi-gpt-${fs}-${loader}"
	    img=${IMGDIR}/${name}.img

	    # For EFI, the stage-3 in the filesystem doesn't matter, use lua variant
	    case ${fs} in
		ufs) fsimg=${IMGDIR}/bootable-ufs-lua.img; ptype="freebsd-ufs" ;;
		zfs) fsimg=${IMGDIR}/bootable-zfs-lua.img; ptype="freebsd-zfs" ;;
	    esac

	    mkimg -s gpt \
		-p efi:=${esp} \
		-p ${ptype}:=${fsimg} \
		-o ${img} >> ${LOGDIR}/imagebuild.log 2>&1

	    register_test ${name} $(qemu_efi ${img})
	done
    done
}

assemble_efi_mbr() {
    echo "  Assembling EFI+MBR images..."
    for loader in $(param efi_loaders); do
	esp=${IMGDIR}/${loader}.esp
	name="efi-mbr-ufs-${loader}"
	img=${IMGDIR}/${name}.img
	ufs=${IMGDIR}/bootable-ufs-lua.img

	mkimg -s bsd -p freebsd-ufs:=${ufs} -o ${img}.s2 >> ${LOGDIR}/imagebuild.log 2>&1
	mkimg -a 1 -s mbr -p efi:=${esp} -p freebsd:=${img}.s2 -o ${img} >> ${LOGDIR}/imagebuild.log 2>&1
	rm -f ${img}.s2

	register_test ${name} $(qemu_efi ${img})
    done
}

assemble_bios_gpt() {
    echo "  Assembling BIOS+GPT images..."
    for variant in $(param bios_loaders); do
	# UFS
	name="bios-gpt-ufs-loader_${variant}"
	img=${IMGDIR}/${name}.img
	ufs=${IMGDIR}/bootable-ufs-${variant}.img

	mkimg -s gpt -b ${DESTDIR}/boot/pmbr \
	    -p freebsd-boot:=${DESTDIR}/boot/gptboot \
	    -p freebsd-ufs:=${ufs} \
	    -o ${img} >> ${LOGDIR}/imagebuild.log 2>&1

	register_test ${name} $(qemu_bios ${img})

	# ZFS
	if has zfs; then
	    name="bios-gpt-zfs-loader_${variant}"
	    img=${IMGDIR}/${name}.img
	    zfs=${IMGDIR}/bootable-zfs-${variant}.img

	    mkimg -s gpt -b ${DESTDIR}/boot/pmbr \
		-p freebsd-boot:=${DESTDIR}/boot/gptzfsboot \
		-p freebsd-zfs:=${zfs} \
		-o ${img} >> ${LOGDIR}/imagebuild.log 2>&1

	    register_test ${name} $(qemu_bios ${img})
	fi
    done
}

assemble_bios_mbr() {
    echo "  Assembling BIOS+MBR images..."
    for variant in $(param bios_loaders); do
	name="bios-mbr-ufs-loader_${variant}"
	img=${IMGDIR}/${name}.img
	ufs=${IMGDIR}/bootable-ufs-${variant}.img

	mkimg -s bsd -b ${DESTDIR}/boot/boot \
	    -p freebsd-ufs:=${ufs} -o ${img}.s1 >> ${LOGDIR}/imagebuild.log 2>&1
	# Note: boot0sio has a longish timeout, and does work but
	# takes longer than 30s so we use mbr.
	mkimg -a 1 -s mbr -b ${DESTDIR}/boot/mbr \
	    -p freebsd:=${img}.s1 -o ${img} >> ${LOGDIR}/imagebuild.log 2>&1
	rm -f ${img}.s1

	register_test ${name} $(qemu_bios ${img})
    done
}

# Hybrid GPT images: ESP + freebsd-boot + filesystem, tested with both BIOS and EFI
assemble_both_gpt() {
    echo "  Assembling hybrid BIOS+EFI GPT images..."
    for l in $(param bios_loaders); do
	loader="loader_${l}"
	esp=${IMGDIR}/${loader}.esp
	fstypes="ufs"
	has zfs && fstypes="ufs zfs"
	for fs in ${fstypes}; do
	    name_base="both-gpt-${fs}-${loader}"
	    img=${IMGDIR}/${name_base}.img

	    case ${fs} in
		ufs)
		    fsimg=${IMGDIR}/bootable-ufs-lua.img
		    ptype="freebsd-ufs"
		    bootblk=${DESTDIR}/boot/gptboot
		    ;;
		zfs)
		    fsimg=${IMGDIR}/bootable-zfs-lua.img
		    ptype="freebsd-zfs"
		    bootblk=${DESTDIR}/boot/gptzfsboot
		    ;;
	    esac

	    mkimg -b ${DESTDIR}/boot/pmbr -s gpt \
		-p efi:=${esp} \
		-p freebsd-boot:=${bootblk} \
		-p ${ptype}:=${fsimg} \
		-o ${img} >> ${LOGDIR}/imagebuild.log 2>&1

	    register_test "${name_base}-bios" $(qemu_bios ${img})
	    register_test "${name_base}-efi" $(qemu_efi ${img})
	done
    done
}

# Hybrid MBR images: ESP + freebsd(boot+ufs), tested with both BIOS and EFI
assemble_both_mbr() {
    echo "  Assembling hybrid BIOS+EFI MBR images..."
    for l in $(param bios_loaders); do
	loader="loader_${l}"
	esp=${IMGDIR}/${loader}.esp
	name_base="both-mbr-ufs-${loader}"
	img=${IMGDIR}/${name_base}.img
	ufs=${IMGDIR}/bootable-ufs-lua.img

	mkimg -s bsd -b ${DESTDIR}/boot/boot \
	    -p freebsd-ufs:=${ufs} -o ${img}.s2 >> ${LOGDIR}/imagebuild.log 2>&1
	mkimg -a 2 -s mbr -b ${DESTDIR}/boot/mbr \
	    -p efi:=${esp} \
	    -p freebsd:=${img}.s2 -o ${img} >> ${LOGDIR}/imagebuild.log 2>&1
	rm -f ${img}.s2

	register_test "${name_base}-bios" $(qemu_bios ${img})
	register_test "${name_base}-efi" $(qemu_efi ${img})
    done
}

assemble_cd() {
    echo "  Assembling CD images..."

    # mtree to add loader and fstab to image
    mt1=$(mktemp ${OUTDIR}/cd-mtree.XXXXXX)
    variant=lua
    cat > ${mt1} <<EOF
./boot/loader type=file mode=0644 contents=${DESTDIR}/boot/loader_${variant}
./etc/fstab type=file mode=0644 contents=${OUTDIR}/fstab.cd
EOF

    # cdboot - BIOS CD boot via El Torito
    name="bios-cd-cdboot"
    img=${IMGDIR}/${name}.iso
    makefs -t cd9660 \
	-o bootimage=i386\;${DESTDIR}/boot/cdboot \
	-o no-emul-boot \
	-o rockridge \
	-o label=FBSDTEST \
	${img} ${mt1} ${DESTDIR} >> ${LOGDIR}/imagebuild.log 2>&1
    register_test ${name} $(qemu_bios_cdrom ${img})

    # isoboot - hybrid BIOS+EFI CD (tests isoboot for BIOS, loader.efi for EFI)
    name="hybrid-cd-isoboot"
    img=${IMGDIR}/${name}.iso
    espfile=$(mktemp ${OUTDIR}/efiboot.XXXXXX)
    make_cd_esp ${espfile} ${DESTDIR}/boot/loader_lua.efi

    makefs -t cd9660 \
	-o bootimage=i386\;${DESTDIR}/boot/cdboot \
	-o no-emul-boot \
	-o bootimage=i386\;${espfile} \
	-o no-emul-boot \
	-o platformid=efi \
	-o rockridge \
	-o label=FBSDTEST \
	${img} ${mt1} ${DESTDIR} >> ${LOGDIR}/imagebuild.log 2>&1

    # Overlay hybrid GPT for isoboot
    imgsize=$(stat -f %z "${img}")

    # Find the EFI partition in the ISO to reference it
    espstart=""
    espsize_cd=""
    for entry in $(etdump --format shell ${img}); do
	eval ${entry}
	if [ "${et_platform}" = "efi" ]; then
	    espstart=$(expr ${et_lba} \* 2048)
	    espsize_cd=$(expr ${et_sectors} \* 512)
	    break
	fi
    done

    if [ -n "${espstart}" ]; then
	hybrid=$(mktemp ${OUTDIR}/hybrid.XXXXXX)
	mkimg -s gpt \
	    --capacity ${imgsize} \
	    -b ${DESTDIR}/boot/pmbr \
	    -p freebsd-boot:=${DESTDIR}/boot/isoboot \
	    -p efi::${espsize_cd}:${espstart} \
	    -o ${hybrid} >> ${LOGDIR}/imagebuild.log 2>&1
	dd if=${hybrid} of=${img} bs=32k count=1 conv=notrunc >> ${LOGDIR}/imagebuild.log 2>&1
	rm -f ${hybrid}
    fi

    rm -f ${espfile}

    # Test the hybrid ISO with both BIOS and EFI
    register_test "${name}-bios" $(qemu_bios_cdrom ${img})
    register_test "${name}-efi" $(qemu_efi_cdrom ${img})

    rm -f ${mt1}
}

assemble_ofw() {
    echo "  Assembling Open Firmware images..."
    # APM partitioned disk with boot1.hfs
    fstypes="ufs"
    has zfs && fstypes="ufs zfs"
    for fs in ${fstypes}; do
	name="ofw-apm-${fs}"
	img=${IMGDIR}/${name}.img

	case ${fs} in
	    ufs) fsimg=${IMGDIR}/bootable-ufs-lua.img; ptype="freebsd-ufs" ;;
	    zfs) fsimg=${IMGDIR}/bootable-zfs-lua.img; ptype="freebsd-zfs" ;;
	esac

	mkimg -a 1 -s apm \
	    -p freebsd-boot:=${DESTDIR}/boot/boot1.hfs \
	    -p ${ptype}:=${fsimg} \
	    -o ${img} >> ${LOGDIR}/imagebuild.log 2>&1

	register_test ${name} $(qemu_ofw ${img})
    done
}

# Open Firmware bootable CD.  Mirrors release/powerpc/mkisoimages.sh: build the
# Apple/OF "macppc" boot image by dd'ing /boot/loader into the hfs-boot block
# at its "Loader START" offset, then hand that to makefs as the El Torito
# no-emul boot image.  OpenBIOS finds it via cd:,\\:tbxi.
assemble_ofw_cd() {
    echo "  Assembling Open Firmware CD image..."
    name="ofw-cd"
    img=${IMGDIR}/${name}.iso

    # cd9660 root fstab overlay
    mt=$(mktemp ${OUTDIR}/ofwcd-mtree.XXXXXX)
    echo "./etc/fstab type=file mode=0644 contents=${OUTDIR}/fstab.cd" > ${mt}

    # Apple/OF boot block with the loader embedded at "Loader START".
    bootblock=$(mktemp ${OUTDIR}/hfs-boot.XXXXXX)
    uudecode -p ${SRCTOP}/release/powerpc/hfs-boot.bz2.uu | bunzip2 > ${bootblock}
    offset=$(hd ${bootblock} | grep 'Loader START' | cut -f 1 -d ' ')
    offset=$((0x${offset} / 512))
    dd if=${DESTDIR}/boot/loader of=${bootblock} seek=${offset} conv=notrunc \
	>> ${LOGDIR}/imagebuild.log 2>&1

    makefs -t cd9660 \
	-o bootimage=macppc\;${bootblock} \
	-o no-emul-boot \
	-o rockridge \
	-o label=FBSDTEST \
	${img} ${mt} ${DESTDIR} >> ${LOGDIR}/imagebuild.log 2>&1

    rm -f ${bootblock} ${mt}
    register_test ${name} $(qemu_ofw_cdrom ${img})
}

# pseries PReP disk boot: MBR with a PReP boot partition (boot1.elf) and the
# UFS root directly on an MBR partition -- no BSD label, matching freebsd-ci
# (whose BSD-slice container does not cross-build from amd64).  SLOF runs
# boot1.elf from the PReP partition, which loads /boot/loader from the UFS.
assemble_prep() {
    echo "  Assembling pseries PReP (MBR) images..."
    name="prep-mbr-ufs"
    img=${IMGDIR}/${name}.img
    ufs=${IMGDIR}/bootable-ufs-lua.img

    mkimg -a 1 -s mbr \
	-p prepboot:=${DESTDIR}/boot/boot1.elf \
	-p freebsd:=${ufs} \
	-o ${img} >> ${LOGDIR}/imagebuild.log 2>&1

    register_test ${name} $(qemu_prep ${img})
}

# pseries CD boot.  SLOF reads \ppc\bootinfo.txt (CHRP boot script) and runs
# the OF loader from the CD.  Mirrors the chrp-boot half of
# release/powerpc/mkisoimages.sh (the macppc/Apple half is mac99-only).
assemble_pseries_cd() {
    echo "  Assembling pseries CD image..."
    name="prep-cd"
    img=${IMGDIR}/${name}.iso

    mt=$(mktemp ${OUTDIR}/pseriescd-mtree.XXXXXX)
    bootinfo=$(mktemp ${OUTDIR}/bootinfo.XXXXXX)
    cat > ${bootinfo} <<EOF
<chrp-boot>
<description>FreeBSD Install</description>
<os-name>FreeBSD</os-name>
<boot-script>boot &device;:,\ppc\chrp\loader</boot-script>
</chrp-boot>
EOF
    cat > ${mt} <<EOF
./etc/fstab type=file mode=0644 contents=${OUTDIR}/fstab.cd
./ppc type=dir mode=0755
./ppc/bootinfo.txt type=file mode=0644 contents=${bootinfo}
./ppc/chrp type=dir mode=0755
./ppc/chrp/loader type=file mode=0644 contents=${DESTDIR}/boot/loader
EOF
    makefs -t cd9660 \
	-o chrp-boot \
	-o rockridge \
	-o label=FBSDTEST \
	${img} ${mt} ${DESTDIR} >> ${LOGDIR}/imagebuild.log 2>&1
    rm -f ${mt} ${bootinfo}
    register_test ${name} $(qemu_prep_cdrom ${img})
}

assemble_linuxboot() {
    echo "  Assembling linuxboot images..."
    esp=${IMGDIR}/linuxboot.esp
    fstypes="ufs"
    has zfs && fstypes="ufs zfs"
    for fs in ${fstypes}; do
	name="linuxboot-gpt-${fs}"
	img=${IMGDIR}/${name}.img

	case ${fs} in
	    ufs) fsimg=${IMGDIR}/bootable-ufs-lua.img; ptype="freebsd-ufs" ;;
	    zfs) fsimg=${IMGDIR}/bootable-zfs-lua.img; ptype="freebsd-zfs" ;;
	esac

	mkimg -s gpt \
	    -p efi:=${esp} \
	    -p ${ptype}:=${fsimg} \
	    -o ${img} >> ${LOGDIR}/imagebuild.log 2>&1

	register_test ${name} $(qemu_efi ${img})
    done
}

# Linuxboot for platforms without an ESP: the kernel+initrd go on the QEMU
# command line, and the disk is just the FreeBSD root filesystem in a GPT
# (no ESP partition).  loader.kboot mounts root from that disk.
assemble_linuxboot_direct() {
    echo "  Assembling linuxboot (direct kernel/initrd) images..."
    linux_kernel=$(find_linux_kernel) || return 0
    initrd=${IMGDIR}/linuxboot-initrd.cpio.gz
    fstypes="ufs"
    has zfs && fstypes="ufs zfs"
    for fs in ${fstypes}; do
	name="linuxboot-${fs}"
	img=${IMGDIR}/${name}.img

	case ${fs} in
	    ufs) fsimg=${IMGDIR}/bootable-ufs-lua.img; ptype="freebsd-ufs" ;;
	    zfs) fsimg=${IMGDIR}/bootable-zfs-lua.img; ptype="freebsd-zfs" ;;
	esac

	mkimg -s gpt \
	    -p ${ptype}:=${fsimg} \
	    -o ${img} >> ${LOGDIR}/imagebuild.log 2>&1

	register_test ${name} $(qemu_linuxboot ${img} ${linux_kernel} ${initrd})
    done
}

# Netboot: stage a TFTP root and register PXE/EFI network-boot tests.  The
# loader fetches the kernel + an md_image RAM root over TFTP and mounts
# /dev/md0.  The bootable UFS image is reused as the RAM root, so the kernel
# must have "options MD_ROOT" (amd64/aarch64 do; riscv64/armv7 GENERIC do
# not -- see the plan).  netboot-bios/netboot-efi go over a real tap(4) +
# dnsmasq (register_netboot_test / netboot_network_setup) so DHCP can carry a
# root-path.
assemble_netboot() {
    echo "  Assembling netboot images..."
    tftp=${OUTDIR}/tftp
    rm -rf ${tftp}
    mkdir -p ${tftp}
    cp -a ${DESTDIR}/boot ${tftp}/boot
    cp ${IMGDIR}/bootable-ufs-lua.img ${tftp}/boot/mdroot.img

    # Serve the kernel and RAM root as .xz: xzfs_fsops (stand/libsa/xzfs.c)
    # transparently retries "<name>.xz" whenever "<name>" isn't found,
    # exactly like gzipfs already does for kernel.gz, so this cuts the bytes
    # that have to cross TFTP with no loader.conf change -- mdroot_name below
    # still names the plain, unsuffixed "mdroot.img", and pxeboot/loader.efi
    # (fetched directly by firmware, not through libsa) are untouched. Both
    # the amd64 BIOS loader (i386/loader/Makefile: LOADER_XZ_SUPPORT) and
    # every EFI loader (efi/loader/conf.c: unconditional) have xzfs compiled
    # in, so netboot-bios and netboot-efi -- which share this tftp tree --
    # both exercise the compressed path.
    #
    # -6 (xz's own default), not -9: -9's 64MB LZMA2 dictionary needs one
    # contiguous vmalloc() of that size before xz_dec_run() can decode
    # anything, and the BIOS loader's *entire* heap is a fixed 64MB
    # (HEAP_MIN in i386/libi386/biosmem.c) regardless of VM memory -- so a
    # 64MB dictionary request against a 64MB heap that already holds other
    # loader state can never succeed (XZ_MEM_ERROR, every read, forever).
    # -6's 8MB dictionary leaves ample headroom and loses ~nothing on data
    # this size.
    xz -6 -f ${tftp}/boot/kernel/kernel
    xz -6 -f ${tftp}/boot/mdroot.img

    # Boot the kernel + RAM root off the network instead of a disk (appended to
    # the arch-correct loader.conf build_tree already wrote).
    cat >> ${tftp}/boot/loader.conf <<EOF
mdroot_load="YES"
mdroot_type="md_image"
mdroot_name="/boot/mdroot.img"
vfs.root.mountfrom="ufs:/dev/md0"
EOF

    # BIOS PXE: the NIC option ROM chainloads pxeboot (the DHCP bootfile).
    # Registered via register_netboot_test, not register_test: these need a
    # real tap(4) + dnsmasq root-path (see netboot_network_setup) rather than
    # slirp, which can't send one.
    #
    # netboot-bios legitimately runs past the default timeout: it's TFTP
    # (not xzfs decode) that's the bottleneck here -- EFI's memdisk=<url>
    # path decodes 150MB .xz images in about a minute, so the loader's xz
    # decoder isn't the bottleneck; this is plain TFTP bandwidth for the
    # kernel+mdroot fetch. Measured wins on real hardware: whole-suite boot
    # time dropped from ~25 min to ~18 min with xzfs in place. TFTP-level
    # fixes (e.g. HTTP instead, once BIOS can do that) are the only further
    # lever; don't chase this again without one.
    if has bios; then
	cp ${DESTDIR}/boot/pxeboot ${tftp}/pxeboot
	register_netboot_test netboot-bios ${tftp} pxeboot bios
    fi
    # EFI: stage loader.efi for the iPXE-chainload test below.  This can't use
    # the NIC's own PXE ROM the way netboot-bios does -- our installed OVMF
    # build (qemu's bundled edk2-x86_64-code.fd) has no NetworkPkg/PXE driver
    # at all (confirmed: zero PXE/HTTPBoot strings in the firmware, and
    # BdsDxe drops straight to "EFI Internal Shell" with no network boot
    # option ever offered, independent of vars pflash/bootindex/-boot n). So
    # EFI netboot instead brings its own iPXE (see the ramdisk block below)
    # to do DHCP/TFTP; netboot-efi's own chain script (also below) omits
    # memdisk= so loader.efi does its own BOOTP call and gets a real
    # DHCP root-path, unlike the RAM-disk tests.
    if has efi; then
	cp ${DESTDIR}/boot/loader_lua.efi ${tftp}/loader.efi
    fi

    # RAM-disk netboot: iPXE (from -hda) chains loader.efi with memdisk=<url>,
    # which the loader boots entirely from RAM -- sidestepping the NFS/root-path
    # problem above.  iPXE EFI is x86-only in the install, so this is gated on
    # netboot_ipxe (amd64 today).  See qemu_netboot_ramdisk / do-memddisk-efi.
    # netboot-efi (real root-path, no memdisk=) piggybacks on the same iPXE
    # image/vars, since it needs the same OVMF-has-no-PXE workaround.
    if [ -n "$(param netboot_ipxe)" ] && have_file "$(param netboot_ipxe)" && \
	    have_file "$(param netboot_efi_vars)"; then
	# Writable copies: QEMU opens -hda and the edk2 vars pflash read-write,
	# but the installed originals are root-owned.
	cp -f $(param netboot_ipxe) ${OUTDIR}/netboot-ipxe.img
	cp -f $(param netboot_efi_vars) ${OUTDIR}/netboot-vars.fd

	if has efi; then
	    cat > ${tftp}/boot-efi.ipxe <<EOF
#!ipxe
chain tftp://\${next-server}/loader.efi
EOF
	    register_netboot_test netboot-efi ${tftp} /boot-efi.ipxe "" qemu_netboot_ramdisk
	fi

	cat > ${tftp}/boot.ipxe <<EOF
#!ipxe
chain tftp://\${next-server}/loader.efi memdisk=tftp://\${next-server}/boot/mdroot.img
EOF
	register_netboot_test netboot-ramdisk ${tftp} /boot.ipxe "" qemu_netboot_ramdisk

	# Same mechanism, but the image is compressed, exercising each codec
	# in stand/efi/loader/decompress.c.  Compress a throwaway copy rather
	# than ${IMGDIR}/bootable-ufs-lua.img itself: gzip/bzip2/xz remove
	# their input on success, and this image is shared with other tests.
	# Compressing a real file (not a pipe) lets zstd embed the frame
	# content size, so zstd_init() can size the output buffer exactly
	# instead of guessing 4x and growing/copying as it decompresses.
	for spec in gzip:gz bzip2:bz2 xz:xz zstd:zst; do
	    tool=${spec%%:*}
	    ext=${spec#*:}
	    img=${tftp}/boot/mdroot-${tool}.img
	    cp ${IMGDIR}/bootable-ufs-lua.img ${img}
	    case ${tool} in
		zstd) zstd -f --rm ${img} >> ${LOGDIR}/imagebuild.log 2>&1 ;;
		*)    ${tool} -f ${img} >> ${LOGDIR}/imagebuild.log 2>&1 ;;
	    esac
	    cat > ${tftp}/boot-${tool}.ipxe <<EOF
#!ipxe
chain tftp://\${next-server}/loader.efi memdisk=tftp://\${next-server}/boot/mdroot-${tool}.img.${ext}
EOF
	    register_netboot_test netboot-ramdisk-${tool} ${tftp} /boot-${tool}.ipxe "" qemu_netboot_ramdisk
	done
    fi

    # BIOS RAM-disk netboot: iPXE loads syslinux memdisk with a *bootable* disk
    # image as its initrd.  memdisk boots the image's MBR and installs a MEMDISK
    # memory disk; the BIOS loader detects it (biosmemdisk.c -> hint.md.0.*), so
    # the kernel gets md0 and roots via the UFS label.  The BIOS loader has no
    # memdisk= arg, hence this different mechanism.
    if has bios && [ -n "$(param netboot_memdisk)" ] && \
	    have_file "$(param netboot_memdisk)"; then
	cp $(param netboot_memdisk) ${tftp}/memdisk
	cp ${IMGDIR}/bios-mbr-ufs-loader_lua.img ${tftp}/bootdisk.img
	cat > ${tftp}/boot-bios.ipxe <<EOF
#!ipxe
kernel tftp://\${next-server}/memdisk
initrd tftp://\${next-server}/bootdisk.img
boot
EOF
	register_netboot_test netboot-bios-memdisk ${tftp} /boot-bios.ipxe bios
    fi
}

# --------------------------------------------------------------------------
# Netboot host networking: vmnet(4) + dnsmasq
#
# QEMU's slirp/user-mode net cannot send DHCP root-path (option 17); without
# one, the loader's netproto defaults to NFS for every file fetch after the
# initial TFTP-delivered bootfile (net_parse_rootpath()/NETPROTO_DEFAULT), which
# is why netboot-bios/ netboot-efi otherwise fail here (no NFS server).
#
# This uses vmnet(4), not tap(4), even though both are clones of the same
# if_tuntap(4) driver and QEMU treats them identically (-netdev tap,ifname=).
# tap(4)'s close handler unconditionally runs if_down()+if_purgeaddrs() on last
# close -- so the interface would lose its address and go down every time QEMU
# exits, and re-adding it needs root. vmnet(4) explicitly skips that, so the
# address assigned once survives every subsequent QEMU open/close.
#
# Creating the interface and giving it an address both require root regardless
# of net.link.tap.user_open (that sysctl only gates opening the cloning device
# itself). So the root setup below runs once, chowns the resulting /dev/vmnetN
# nodes to the invoking user (so QEMU, unprivileged, can open them directly --
# see net/tap-bsd.c: it opens /dev/<ifname> directly when that device already
# exists, only falling back to the /dev/tap cloning device otherwise), and
# leaves everything running. Later invocations detect the existing state via a
# content hash and skip sudo entirely: "once per boot", not "once per run".
# --------------------------------------------------------------------------

# Deterministically slice a /30 out of $(param netboot_subnet_base) (a /16,
# e.g. 198.18.0.0) for plan line ${1} (0-based). Prints "gw client".
netboot_subnet_for() {
    idx=$1
    base=$(param netboot_subnet_base)
    o1o2=$(echo ${base} | cut -d. -f1-2)
    o3=$(($(echo ${base} | cut -d. -f3) + idx / 64))
    o4=$(((idx % 64) * 4))
    echo "${o1o2}.${o3}.$((o4 + 1)) ${o1o2}.${o3}.$((o4 + 2))"
}

# Runs after every arch is built (so ${NETBOOT_PLAN} is complete) and before
# run_all_tests. Assigns each registered netboot test its own vmnet + /30,
# escalates via sudo only if the host doesn't already match that plan, then
# patches the __NETBOOT_TAP__ placeholder in each test's command file.
netboot_network_setup() {
    [ -s "${NETBOOT_PLAN}" ] || return 0
    echo "=== Netboot: configuring vmnet(4) + dnsmasq ==="
    mkdir -p "${NETBOOT_STATE_DIR}"

    resolved=${NETBOOT_STATE_DIR}/plan.new
    : > "${resolved}"
    idx=0
    while read cmdfile tftpdir bootfile; do
	vmnet="vmnet$((100 + idx))"	# offset away from any operator-managed vmnets
	set -- $(netboot_subnet_for ${idx})
	gw=$1
	client=$2
	echo "${vmnet} ${gw} ${client} 30 ${tftpdir} ${bootfile} ${cmdfile}" >> "${resolved}"
	idx=$((idx + 1))
    done < "${NETBOOT_PLAN}"

    newhash=$(md5 -q "${resolved}")
    oldhash=""
    [ -f "${NETBOOT_STATE_DIR}/state.hash" ] && oldhash=$(cat "${NETBOOT_STATE_DIR}/state.hash")

    # dnsmasq runs as "nobody" (it drops root after binding), so kill -0
    # from this unprivileged process would always fail with EPERM even when
    # it's alive; check process existence via ps instead, which doesn't
    # require signal permission.
    converged=false
    if [ "${newhash}" = "${oldhash}" ] && [ -s "${NETBOOT_STATE_DIR}/dnsmasq.pid" ] \
	    && ps -p "$(cat ${NETBOOT_STATE_DIR}/dnsmasq.pid)" > /dev/null 2>&1; then
	converged=true
	while read vmnet gw client prefix tftpdir bootfile cmdfile; do
	    ifconfig "${vmnet}" > /dev/null 2>&1 || { converged=false; break; }
	done < "${resolved}"
    fi

    if ${converged}; then
	echo "  Existing vmnet(4)/dnsmasq setup already matches -- no sudo needed."
    else
	echo "  Host networking missing or stale; requesting sudo once to (re)create it..."
	cp "${resolved}" "${NETBOOT_STATE_DIR}/plan"
	sudo "$0" --netboot-helper "${NETBOOT_STATE_DIR}/plan" \
	    || die "netboot vmnet(4)/dnsmasq setup (sudo) failed"
    fi

    while read vmnet gw client prefix tftpdir bootfile cmdfile; do
	sed -i '' "s/__NETBOOT_TAP__/${vmnet}/" "${cmdfile}"
    done < "${resolved}"
}

# Root-side setup, only reached via `sudo "$0" --netboot-helper <planfile>`
# (see netboot_network_setup). ${1} has the same "vmnet gw client prefix
# tftpdir bootfile cmdfile" lines as netboot_network_setup's resolved plan.
netboot_helper() {
    planfile=$1
    [ -r "${planfile}" ] || die "netboot helper: cannot read plan ${planfile}"
    invoker=${SUDO_UID:-$(id -u)}
    mkdir -p "${NETBOOT_STATE_DIR}"

    # Converge from scratch rather than trusting the caller's hash check:
    # tear down anything left over from a previous (possibly interrupted) run.
    if [ -s "${NETBOOT_STATE_DIR}/dnsmasq.pid" ]; then
	kill "$(cat ${NETBOOT_STATE_DIR}/dnsmasq.pid)" 2>/dev/null || true
	rm -f "${NETBOOT_STATE_DIR}/dnsmasq.pid"
    fi
    for vmnet in $(ifconfig -g boot-test 2>/dev/null); do
	ifconfig "${vmnet}" destroy
    done

    conf=${NETBOOT_STATE_DIR}/dnsmasq.conf
    cat > "${conf}" <<EOF
port=0
bind-interfaces
enable-tftp
log-dhcp
pid-file=${NETBOOT_STATE_DIR}/dnsmasq.pid
dhcp-leasefile=${NETBOOT_STATE_DIR}/dnsmasq.leases

# QEMU's PXE ROM is iPXE; it self-identifies as vendor-class
# "PXEClient:Arch:00000:UNDI:002001" (stand/libsa/bootp.c's own request just
# says "PXEClient", no ":Arch:..." suffix, so this substring match is
# iPXE-only). iPXE's autoboot() gives DHCP root-path priority over
# chainloading the DHCP filename -- if root-path is set it tries to
# sanboot(8) it instead, which fails outright for a tftp:// URI ("Could not
# open SAN device"). So root-path below is withheld from iPXE's own
# negotiation and only given once stand/libsa/bootp.c does its own separate
# BOOTP call after pxeboot/loader.efi has already been chainloaded.
dhcp-vendorclass=set:ipxerom,PXEClient:Arch
EOF

    while read vmnet gw client prefix tftpdir bootfile cmdfile; do
	ifconfig "${vmnet}" create group boot-test
	ifconfig "${vmnet}" inet "${gw}/${prefix}" up
	chown "${invoker}" "/dev/${vmnet}"

	# dnsmasq's dhcp-boot only sets DHCP option 67 (bootfile-name), never
	# the classic fixed-length BOOTP "file" field -- confirmed by comparing
	# against QEMU's own slirp DHCP server, which sets that field directly
	# and gets a plain "Filename: pxeboot" chainload with no further
	# ceremony. Without it, and since iPXE requested option 43 (PXE vendor
	# info) in its Parameter-Request list, iPXE assumes it must run the full
	# PXE boot-server-discovery dance instead of trusting option 67 -- seen
	# on the wire as a TFTP RRQ with an empty filename, then "No
	# configuration methods succeeded" / "PXEBS ...  Connection timed
	# out". Telling it not to via PXE discovery-control (option 43
	# sub-option 6, value 8 = "use bootfile name from the DHCP packet, don't
	# discover") fixes it directly, without dnsmasq's heavier --pxe-service
	# boot-menu machinery (which defaults discovery-control to 3, still
	# triggering discovery).
	cat >> "${conf}" <<EOF
interface=${vmnet}
dhcp-range=set:${vmnet},${client},${client},255.255.255.252,1h
dhcp-boot=tag:${vmnet},${bootfile}
dhcp-option=tag:${vmnet},tag:ipxerom,encap:43,6,8
dhcp-option=tag:${vmnet},tag:!ipxerom,17,"tftp://${gw}/"
tftp-root=${tftpdir},${vmnet}
EOF
    done < "${planfile}"

    dnsmasq --conf-file="${conf}"

    # dnsmasq daemonizes itself; wait for the pidfile before trusting it.
    tries=0
    while [ ! -s "${NETBOOT_STATE_DIR}/dnsmasq.pid" ] && [ ${tries} -lt 5 ]; do
	sleep 1
	tries=$((tries + 1))
    done
    [ -s "${NETBOOT_STATE_DIR}/dnsmasq.pid" ] || die "dnsmasq did not start"

    md5 -q "${planfile}" > "${NETBOOT_STATE_DIR}/state.hash"
    chown "${invoker}" "${NETBOOT_STATE_DIR}" "${conf}" "${planfile}" \
	"${NETBOOT_STATE_DIR}/dnsmasq.pid" "${NETBOOT_STATE_DIR}/state.hash"
}

# Manual cleanup: `sudo sh boot-test.sh --netboot-teardown`. Not run
# automatically -- leaving the setup running is what makes it "once per
# boot" instead of "once per run" (see netboot_network_setup).
netboot_teardown() {
    if [ -s "${NETBOOT_STATE_DIR}/dnsmasq.pid" ]; then
	kill "$(cat ${NETBOOT_STATE_DIR}/dnsmasq.pid)" 2>/dev/null || true
    fi
    for vmnet in $(ifconfig -g boot-test 2>/dev/null); do
	ifconfig "${vmnet}" destroy
    done
    rm -rf "${NETBOOT_STATE_DIR}"
    echo "Netboot vmnet(4)/dnsmasq setup torn down."
}

assemble_all_images() {
    echo "=== Phase 3: Assembling disk images ==="

    if has efi; then
	if [ -r "$(param efi_firmware)" ]; then
	    assemble_efi_gpt
	    if has mbr; then
		assemble_efi_mbr
	    fi
	elif [ -n "$(param efi_firmware)" ]; then
	    echo "WARNING: EFI firmware not found at $(param efi_firmware), skipping EFI tests"
	else
	    # riscv64 uses u-boot with EFI payload
	    assemble_efi_gpt
	fi
    fi

    if has bios; then
	assemble_bios_gpt
	if has mbr; then
	    assemble_bios_mbr
	fi
    fi

    if has bios && has efi && [ -r "$(param efi_firmware)" ]; then
	assemble_both_gpt
	if has mbr; then
	    assemble_both_mbr
	fi
    fi

    if has cd; then
	if has prep; then
	    assemble_pseries_cd		# pseries SLOF CHRP CD
	elif has ofw; then
	    assemble_ofw_cd		# mac99 Apple/OF CD
	else
	    assemble_cd			# x86 El Torito
	fi
    fi

    if has prep; then
	assemble_prep
    fi

    # Used for mac99 emulation, though kernel issues prevent testing
    if has ofw; then
	assemble_ofw
    fi

    if has linuxboot; then
	if [ -f "${IMGDIR}/linuxboot.esp" ]; then
	    assemble_linuxboot
	elif [ -f "${IMGDIR}/linuxboot-initrd.cpio.gz" ]; then
	    assemble_linuxboot_direct
	fi
    fi

    if has netboot; then
	assemble_netboot
    fi
}

# --------------------------------------------------------------------------
# Phase 4: Run tests in parallel
# --------------------------------------------------------------------------

run_one_test() {
    name=$1
    log="${LOGDIR}/${name}.log"
    cmd=$(cat ${OUTDIR}/test-cmd-${name}.sh)

    expect -c "
	set timeout ${TIMEOUT}
	log_file -noappend \"${log}\"
	spawn {*}${cmd}
	expect {
	    \"SUCCESS\" { exit 0 }
	    timeout    { exit 1 }
	    eof        { exit 2 }
	}
    " >/dev/null 2>&1
    return $?
}

wait_for_slot() {
    # Wait until fewer than MAX_JOBS are running
    while true; do
	running=0
	for p in ${active_pids}; do
	    if kill -0 $p 2>/dev/null; then
		running=$((running + 1))
	    fi
	done
	[ ${running} -lt ${MAX_JOBS} ] && break
	sleep 1
    done
}

# Launch every arch's tests together and wait once, so a run of N arches costs
# roughly one timeout rather than N.  Each `run_one_test &` snapshots the
# per-arch OUTDIR/LOGDIR/TIMEOUT that setup_arch_env just set, so backgrounded
# jobs keep their own arch context even as we move on to the next arch.
run_all_tests() {
    echo "=== Phase 4: Running tests ==="

    total=0
    skipped=0
    active_pids=""
    results_map=$(mktemp -t boot-test-results)

    for arch in ${ARCHES}; do
	setup_arch_env "${arch}"
	[ -s "${TESTLIST}" ] || continue

	while read name; do
	    # Apply test filter if given
	    if [ -n "${TEST_FILTER}" ]; then
		echo "${name}" | grep -qE "${TEST_FILTER}" || {
		    skipped=$((skipped + 1))
		    continue
		}
	    fi

	    total=$((total + 1))

	    # Job throttling (global across all arches)
	    if [ ${MAX_JOBS} -gt 0 ]; then
		wait_for_slot
	    fi

	    run_one_test "${name}" &
	    pid=$!
	    active_pids="${active_pids} ${pid}"
	    echo "${pid} ${arch} ${name}" >> ${results_map}
	done < ${TESTLIST}
    done

    [ ${total} -gt 0 ] || die "No tests registered. Run without -B first."
    echo "  ${total} tests launched (${skipped} skipped by filter), waiting..."
    echo ""

    # Collect results - disable errexit since wait returns the child's exit status
    set +e
    pass=0
    fail=0
    timeout_count=0
    while read pid arch name; do
	wait ${pid}
	rc=$?
	case ${rc} in
	    0)
		result="PASS"
		pass=$((pass + 1))
		;;
	    1)
		result="TIMEOUT"
		timeout_count=$((timeout_count + 1))
		;;
	    *)
		result="FAILED"
		fail=$((fail + 1))
		;;
	esac
	printf "  %-12s %-40s %s\n" "${arch}" "${name}" "${result}"
    done < ${results_map}

    echo ""
    echo "=== Results: ${pass} passed, ${fail} failed, ${timeout_count} timed out (of ${total}) ==="

    rm -f ${results_map}
    [ ${fail} -eq 0 ] && [ ${timeout_count} -eq 0 ]
}

# Build the tree and images for each arch, one arch at a time.  Each arch runs
# in a subshell so its setup_arch_env globals stay isolated and a build failure
# (set -e) is caught here instead of aborting the whole run.
build_all() {
    for arch in ${ARCHES}; do
	echo "############################################################"
	echo "# ${arch}: building"
	echo "############################################################"
	if ! (
	    setup_arch_env "${arch}"
	    need_cmd "$(param qemu_bin)"
	    ${SKIP_BUILD} || build_tree
	    if ! ${SKIP_IMAGES}; then
		[ -d "${DESTDIR}" ] || \
		    die "No boot tree at ${DESTDIR}. Run without -b first."
		: > ${TESTLIST}		# fresh test list before (re)assembling
		make_base_images
		assemble_all_images
	    fi
	); then
	    echo "  ${arch}: BUILD FAILED"
	fi
    done
}

# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------

# --netboot-helper/--netboot-teardown are internal/manual entry points that
# skip the whole build+test flow below; see netboot_network_setup.
case "${NETBOOT_MODE}" in
    helper)   netboot_helper "${NETBOOT_HELPER_PLAN}"; exit $? ;;
    teardown) netboot_teardown; exit $? ;;
esac

# Preflight: universal tools (per-arch qemu binaries are checked in build_all).
for prog in jq expect makefs mkimg; do
    need_cmd "${prog}"
done

echo "FreeBSD boot loader test suite: ${ARCHES}"
echo ""

# Accumulates netboot test plan lines across every arch (unlike ${TESTLIST},
# which build_all truncates per-arch) -- see register_netboot_test.
NETBOOT_PLAN=$(mktemp -t boot-test-netboot-plan)

if $do_report_dirs; then
    for arch in ${ARCHES}; do
	setup_arch_env "${arch}"
	echo "${arch} settings:"
	echo "    TIMEOUT=${TIMEOUT}"
	echo "    ARCH_OBJDIR=${ARCH_OBJDIR}"
	echo "    OUTDIR=${OUTDIR}"
	echo "    LOGDIR=${LOGDIR}"
	echo "    DESTDIR=${DESTDIR}"
	echo "    TESTLIST=${TESTLIST}"
    done
    exit 0
fi

build_all
netboot_network_setup
if run_all_tests; then
    rc=0
else
    rc=$?
fi
rm -f "${NETBOOT_PLAN}"
exit ${rc}
