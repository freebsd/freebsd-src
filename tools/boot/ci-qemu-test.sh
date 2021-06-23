#!/bin/sh

# Install pkgbase packages for loader, kernel, and enough of userland to boot
# in QEMU and echo "Hello world." from init, as a very quick smoke test for CI.
# Uses QEMU's virtual FAT filesystem to avoid the need to create a disk image.
# While designed for CI automated testing, this script can also be run by hand
# as a quick smoke-test as long as pkgbase packages have been built.  The
# rootgen.sh and related scripts generate much more extensive tests for many
# combinations of boot env (ufs, zfs, geli, etc).
#
# $FreeBSD$

set -e

die()
{
	echo "$*" 1>&2
	exit 1
}

tempdir_cleanup()
{
	trap - EXIT SIGINT SIGHUP SIGTERM SIGQUIT
	rm -rf ${ROOTDIR}
}

tempdir_setup()
{
	# Create minimal directory structure and populate it.

	for dir in dev bin efi/boot etc lib libexec sbin usr/lib usr/libexec; do
		mkdir -p ${ROOTDIR}/${dir}
	done

	# Install kernel, loader and minimal userland.
	cat<<EOF >${ROOTDIR}/pkg.conf
REPOS_DIR=[]
repositories={local {url = file://$(dirname $OBJTOP)/repo/\${ABI}/latest}}
EOF
	ASSUME_ALWAYS_YES=true INSTALL_AS_USER=true pkg \
	    -o ABI_FILE=$OBJTOP/bin/sh/sh \
	    -C ${ROOTDIR}/pkg.conf -r ${ROOTDIR} install \
	    FreeBSD-kernel-generic FreeBSD-bootloader \
	    FreeBSD-clibs FreeBSD-runtime

	# Put loader in standard EFI location.
	mv ${ROOTDIR}/boot/loader.efi ${ROOTDIR}/efi/boot/$EFIBOOT

	# Configuration files.
	cat > ${ROOTDIR}/boot/loader.conf <<EOF
vfs.root.mountfrom="msdosfs:/dev/$ROOTDEV"
autoboot_delay=-1
boot_verbose=YES
EOF
	cat > ${ROOTDIR}/etc/rc <<EOF
#!/bin/sh

echo "Hello world."
/sbin/sysctl vm.stats.vm.v_wire_count
/sbin/shutdown -p now
EOF

	# Entropy needed to boot, see r346250 and followup commits/discussion.
	dd if=/dev/random of=${ROOTDIR}/boot/entropy bs=4k count=1

	# Remove unnecessary files to keep FAT filesystem size down.
	rm -rf ${ROOTDIR}/METALOG ${ROOTDIR}/usr/lib
}

# Locate the top of the source tree, to run make install from.
: ${SRCTOP:=$(make -V SRCTOP)}
if [ -z "${SRCTOP}" ]; then
	die "Cannot locate top of source tree"
fi
: ${OBJTOP:=$(make -V OBJTOP)}
if [ -z "${OBJTOP}" ]; then
	die "Cannot locate top of object tree"
fi

: ${TARGET:=$(uname -m)}
case $TARGET in
amd64)
	# Locate the uefi firmware file used by qemu.
	: ${OVMF:=/usr/local/share/qemu/edk2-x86_64-code.fd}
	if [ ! -r "${OVMF}" ]; then
		die "Cannot read UEFI firmware file ${OVMF}"
	fi
	QEMU="qemu-system-x86_64 -drive if=pflash,format=raw,readonly,file=${OVMF}"
	EFIBOOT=BOOTx64.EFI
	ROOTDEV=ada0s1
	;;
arm64)
	QEMU="qemu-system-aarch64 -cpu cortex-a57 -M virt -bios edk2-aarch64-code.fd"
	EFIBOOT=BOOTAA64.EFI
	ROOTDEV=vtbd0s1
	;;
*)
	die "Unknown TARGET:TARGET_ARCH $TARGET:$TARGET_ARCH"
esac

# Create a temp dir to hold the boot image.
ROOTDIR=$(mktemp -d -t ci-qemu-test-fat-root)
trap tempdir_cleanup EXIT SIGINT SIGHUP SIGTERM SIGQUIT

# Populate the boot image in a temp dir.
( cd ${SRCTOP} && tempdir_setup )

# And, boot in QEMU.
: ${BOOTLOG:=${TMPDIR:-/tmp}/ci-qemu-test-boot.log}
timeout 300 \
    $QEMU -m 256M -nodefaults \
        -serial stdio -vga none -nographic -monitor none \
        -snapshot -hda fat:${ROOTDIR} 2>&1 | tee ${BOOTLOG}

# Check whether we succesfully booted...
if grep -q 'Hello world.' ${BOOTLOG}; then
	echo "OK"
else
	die "Did not boot successfully, see ${BOOTLOG}"
fi
