#!/bin/sh

# STAND_ROOT is the root of a tree:
# cache - Cached binaries that we have downloaded
# trees - binary trees that we use to make image
#      trees/${ARCH}/$thing
# images - bootable images that we use to test
#	images/${ARCH}/$thing
# bios - cached bios images (as well as 'vars' files when we start testing
#	different booting scenarios in the precense / absence of variables).
# scripts - generated scripts that uses images to run the tests.
#
# Strategy:
#	Download FreeBSD release isos, Linux kernels (for the kboot tests) and
#	other misc things. We use these to generate dozens of test images that we
#	use qemu-system-XXXX to boot. They all boot the same thing at the moment:
#	an /etc/rc script that prints the boot method, echos success and then
#	halts.

# What version of FreeBSD to we snag the ISOs from to extract the binaries
# we are testing
FREEBSD_VERSION=14.2
# eg https://download.freebsd.org/releases/amd64/amd64/ISO-IMAGES/14.2/FreeBSD-14.2-RELEASE-amd64-bootonly.iso.xz
URLBASE="https://download.freebsd.org/releases"
: ${STAND_ROOT:="${HOME}/stand-test-root"}
CACHE=${STAND_ROOT}/cache
TREES=${STAND_ROOT}/trees
IMAGES=${STAND_ROOT}/images
BIOS=${STAND_ROOT}/bios
SCRIPTS=${STAND_ROOT}/scripts
OVERRIDE=${STAND_ROOT}/override

# Find make
case $(uname) in
    Darwin)
	t=$(realpath $(dirname $0)/../..)
	# Use the python wrapper to find make
	if [ -f ${t}/tools/build/make.py ]; then
	    MAKE="${t}/tools/build/make.py"
	    case $(uname -m) in
		arm64)
		    DEFARCH="TARGET_ARCH=aarch64 TARGET=arm64"
		    ;;
		x86_64)
		    DEFARCH="TARGET_ARCH=amd64 TARGET=amd64"
		    ;;
		*)
		    die "Do not know about $(uanme -p)"
		    ;;
	    esac
	else
	    die "Can't find the make wrapper"
	fi
	qemu_bin=/opt/homebrew/bin
	;;
    FreeBSD)
	MAKE=make
	qemu_bin=/usr/local/bin
	;;
    # linux) not yet
    *)
	die "Do not know about system $(uname)"
	;;
esac

SRCTOP=$(${MAKE} ${DEFARCH} -v SRCTOP)
echo $SRCTOP

# Find makefs and mkimg
MAKEFS=$(SHELL="which makefs" ${MAKE} ${DEFARCH} buildenv | tail -1) || die "No makefs try WITH_DISK_IMAGE_TOOLS_BOOTSTRAP=y"
MKIMG=$(SHELL="which mkimg" ${MAKE} ${DEFARCH} buildenv | tail -1) || die "No mkimg, try buildworld first"
MTREE=$(SHELL="which mtree" ${MAKE} ${DEFARCH} buildenv | tail -1) || die "No mtree, try buildworld first"

# MAKE=$(SHELL="which make" ${MAKE} ${DEFARCH} buildenv | tail -1) || die "No make, try buildworld first"


# All the architectures under test
# Note: we can't yet do armv7 because we don't have a good iso for it and would
# need root to extract the files.
#ARCHES="amd64:amd64 i386:i386 powerpc:powerpc powerpc:powerpc64 powerpc:powerpc64le powerpc:powerpcspe arm64:aarch64 riscv:riscv64"
ARCHES="amd64:amd64 arm64:aarch64"

# The smallest FAT32 filesystem is 33292 KB
espsize=33292

mkdir -p ${CACHE} ${TREES} ${IMAGES} ${BIOS}

die()
{
    echo Fatal Error: $*
    exit 1
}

ma_combo()
{
    local m=$1
    local ma=$2
    local ma_combo="${m}"

    [ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
    echo ${ma_combo}
}

fetch_one()
{
    local m=$1
    local ma=$2
    local v=$3
    local flavor=$4
    local ma_combo=$(ma_combo $m $ma)
    local file="FreeBSD-${v}-RELEASE-${ma_combo}-${flavor}"
    local url="${URLBASE}/${m}/${ma}/ISO-IMAGES/${v}/${file}.xz"

    mkdir -p ${CACHE}
    [ -r ${CACHE}/${file} ] && echo "Using cached ${file}" && return
    cd ${CACHE}
    echo "Fetching ${url}"
    fetch ${url} || die "Can't fetch ${file} from ${url}"
    xz -d ${file}.xz || die "Can't uncompress ${file}.xz"
    cd ..
}

update_freebsd_img_cache()
{
    local a m ma

    for a in $ARCHES; do
	m=${a%%:*}
	ma=${a##*:}
	fetch_one $m $ma ${FREEBSD_VERSION} bootonly.iso
    done

    fetch_one arm armv7 ${FREEBSD_VERSION} GENERICSD.img
}

make_minimal_freebsd_tree()
{
    local m=$1
    local ma=$2
    local v=$3
    local flavor=$4
    local file d
    local ma_combo="${m}"
    [ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"

    file="FreeBSD-${v}-RELEASE-${ma_combo}-${flavor}"
    dir=${TREES}/${ma_combo}/freebsd
    rm -rf ${dir}

    # Make a super simple userland. It has just enough to print a santiy value,
    # then say test succeeded, and then halt the system. We assume that /bin/sh
    # has all the library prereqs for the rest...
    mkdir -p ${dir}
    # Make required dirs
    for d in boot/kernel boot/defaults boot/lua boot/loader.conf.d \
			 sbin bin lib libexec etc dev; do
	mkdir -p ${dir}/${d}
    done
    # Pretend we don't have a separate /usr
    ln -s . ${dir}/usr
    # snag the binaries for my simple /etc/rc file
    tar -C ${dir} -xf ${CACHE}/$file sbin/fastboot sbin/reboot sbin/halt sbin/init bin/sh sbin/sysctl \
	lib/libtinfow.so.9 lib/libncursesw.so.9 lib/libc.so.7 lib/libedit.so.8 libexec/ld-elf.so.1
    # My simple etc/rc
    cat > ${dir}/etc/rc <<EOF
#!/bin/sh

sysctl machdep.bootmethod
echo "RC COMMAND RUNNING -- SUCCESS!!!!!"
halt -p
EOF
    chmod +x ${dir}/etc/rc

    # Check to see if we have overrides here... So we can insert our own kernel
    # instead of the one from the release.
    echo "CHECKING ${OVERRIDE}/${ma_combo}/boot"
    if [ -d ${OVERRIDE}/${ma_combo}/boot ]; then
	o=${OVERRIDE}/${ma_combo}
	for i in \
	    boot/device.hints \
	    boot/kernel/kernel \
	    boot/kernel/acl_nfs4.ko \
	    boot/kernel/cryptodev.ko \
	    boot/kernel/zfs.ko \
	    boot/kernel/geom_eli.ko; do
	    [ -r $o/$i ] && echo Copying override $i && cp $o/$i ${dir}/$i
	done
    else
	# Copy the kernel (but not the boot loader, we'll add the one to test later)
	# This will take care of both UFS and ZFS boots as well as geli
	# Note: It's OK for device.hints to be missing. It's mostly for legacy platforms.
	tar -C ${dir} -xf ${CACHE}/$file \
	    boot/device.hints \
	    boot/kernel/kernel \
	    boot/kernel/acl_nfs4.ko \
	    boot/kernel/cryptodev.ko \
	    boot/kernel/zfs.ko \
	    boot/kernel/geom_eli.ko || true
	# XXX WHAT TO DO ABOUT LINKER HINTS -- PUNT FOR NOW
	# XXX also, ZFS not supported on 32-bit powerpc platforms
    fi

    # Setup some common settings for serial console, etc
    echo -h -D -S115200 > ${dir}/boot.config
    cat > ${dir}/boot/loader.conf <<EOF
comconsole_speed=115200
autoboot_delay=2
zfs_load="YES"
boot_verbose=yes
kern.cfg.order="acpi,fdt"
boot_serial="YES"
hw.uart.console="io:1016,br:115200"
vfs.root.mountfrom="ufs:/dev/ufs/root"
vfs.root.mountfrom.options="rw"
EOF
}

make_freebsd_minimal_trees()
{
    for a in $ARCHES; do
	m=${a%%:*}
	ma=${a##*:}
	make_minimal_freebsd_tree $m $ma ${FREEBSD_VERSION} bootonly.iso
    done
    # Note: armv7 isn't done yet as its the odd-man out -- we need to extract things
    # in a special way, so punt for the moment
}

make_freebsd_test_trees()
{
    for a in $ARCHES; do
	m=${a%%:*}
	ma=${a##*:}
	ma_combo="${m}"
	[ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
	dir=${TREES}/${ma_combo}/test-stand
	mkdir -p ${dir}
	${MTREE} -deUW -f ${SRCTOP}/etc/mtree/BSD.root.dist -p ${dir}
	echo "Creating tree for ${m}:${ma}"
	cd ${SRCTOP}
	# Indirection needed because our build system is too complex
	# Also, bare make for 'inside' the buildenv ${MAKE} for outside
#	SHELL="make clean" ${MAKE} buildenv TARGET=${m} TARGET_ARCH=${ma}
	SHELL="sh -c 'cd stand ; make -j 100 all'" ${MAKE} TARGET=${m} TARGET_ARCH=${ma} buildenv
	DESTDIR=${dir} SHELL="sh -c 'cd stand ; make install MK_MAN=no MK_INSTALL_AS_USER=yes WITHOUT_DEBUG_FILES=yes'" \
	     ${MAKE} buildenv TARGET=${m} TARGET_ARCH=${ma}
	rm -rf ${dir}/bin ${dir}/[ac-z]*	# Don't care about anything here
    done
}

make_linux_initrds()
{
    # At the moment, we have just two
    for a in amd64:amd64 arm64:aarch64; do
	m=${a%%:*}
	ma=${a##*:}
	ma_combo="${m}"
	[ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
	dir=${TREES}/${ma_combo}/linuxboot
	dir2=${TREES}/${ma_combo}/test-stand
	dir3=${TREES}/${ma_combo}/freebsd
	initrd=${TREES}/${ma_combo}/initrd.img
	rm -rf ${dir}
	mkdir -p ${dir}
	cp ${dir2}/boot/loader.kboot ${dir}/init
	# Copy the boot loader
	tar -c -f - -C ${dir2} boot | tar -xf - -C ${dir}
	# Copy the boot kernel
	tar -c -f - -C ${dir3} boot | tar -xf - -C ${dir}
	(cd ${dir} ; find . | LC_ALL=C sort | cpio -o -H newc | gzip > ${initrd})
    done
}

make_linux_esps()
{
    # At the moment, we have just two
    for a in amd64:amd64 arm64:aarch64; do
	m=${a%%:*}
	ma=${a##*:}
	ma_combo="${m}"
	[ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
	dir=${TREES}/${ma_combo}/linuxboot-esp
	initrd=${TREES}/${ma_combo}/initrd.img
	mkdir -p ${dir}
	case ${ma} in
	    amd64) bin=x64 cons="console=ttyS0,115200" ;;
	    aarch64) bin=aa64 ;;
	esac
	mkdir -p ${dir}/efi/boot
	cp ${CACHE}/linux/linux${bin}.efi ${dir}
	cp ${CACHE}/linux/shell${bin}.efi ${dir}/efi/boot/boot${bin}.efi
	cat > ${dir}/startup.nsh <<EOF
# Run linux
# Tell it to run with out special initrd that then boot FreeBSD

\linux${bin} ${cons} initrd=\initrd.img
EOF
	cp $initrd ${dir}
    done
}

make_linuxboot_images()
{
    # ESP variant: In this variant, amd64 and arm64 are both created more or
    # less the same way. Both are EFI + ACPI implementations
    for a in amd64:amd64 arm64:aarch64; do
	m=${a%%:*}
	ma=${a##*:}
	ma_combo="${m}"
	[ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
	src=${TREES}/${ma_combo}/linuxboot-esp
	dir=${TREES}/${ma_combo}/freebsd
	dir2=${TREES}/${ma_combo}/test-stand
	esp=${IMAGES}/${ma_combo}/linuxboot-${ma_combo}.esp
	ufs=${IMAGES}/${ma_combo}/linuxboot-${ma_combo}.ufs
	zfs=${IMAGES}/${ma_combo}/linuxboot-${ma_combo}.zfs
	img=${IMAGES}/${ma_combo}/linuxboot-${ma_combo}.img
	img2=${IMAGES}/${ma_combo}/linuxboot-${ma_combo}-zfs.img
	pool="linuxboot"
	mkdir -p ${IMAGES}/${ma_combo}
	${MAKEFS} -t msdos -o fat_type=32 -o sectors_per_cluster=1 \
	       -o volume_label=EFISYS -s80m ${esp} ${src}
	${MAKEFS} -t ffs -B little -s 200m -o label=root ${ufs} ${dir} ${dir2}
	${MKIMG} -s gpt -p efi:=${esp} -p freebsd-ufs:=${ufs} -o ${img}
	${MAKEFS} -t zfs -s 200m \
	       -o poolname=${pool} -o bootfs=${pool} -o rootpath=/ \
		${zfs} ${dir} ${dir2}
	${MKIMG} -s gpt \
	      -p efi:=${esp} \
	      -p freebsd-zfs:=${zfs} -o ${img2}
	rm -f ${esp}	# Don't need to keep this around
    done

    # The raw variant, currently used only on arm64. It boots with the raw interface of qemu
    # for testing purposes.  This means it makes a good test for the DTB variation, but not ACPI
    # since qemu doesn't currently provide that...
    for a in arm64:aarch64; do
	m=${a%%:*}
	ma=${a##*:}
	ma_combo="${m}"
	[ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
	linux="${CACHE}/linux/vmlinux-${m}*"
	initrd=${TREES}/${ma_combo}/initrd.img
	img=${IMAGES}/${ma_combo}/linuxboot-${ma_combo}-raw
	cp ${linux} ${img}.kernel
	cp ${initrd} ${img}.initrd
    done
}

make_linuxboot_scripts()
{
    # At the moment, we have just two -- and the images we've built so far are just
    # the hostfs boot. The boot off anything more complex isn't here.
    for a in amd64:amd64 arm64:aarch64; do
	m=${a%%:*}
	ma=${a##*:}
	ma_combo="${m}"
	[ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"

	# First off, update the edk firmware
	bios_code=${BIOS}/edk2-${ma_combo}-code.fd
	bios_vars=${BIOS}/edk2-${ma_combo}-vars.fd
	case ${ma} in
	    amd64)
		if [ ${bios_code} -ot /usr/local/share/qemu/edk2-x86_64-code.fd ]; then
		    cp /usr/local/share/qemu/edk2-x86_64-code.fd ${bios_code}
		    # vars file works on both 32 and 64 bit x86
#		    cp /usr/local/share/qemu/edk2-i386-vars.fd ${bios_vars}
		fi
		;;
	    aarch64)
		if [ ${bios_code} -ot /usr/local/share/qemu/edk2-aarch64-code.fd ]; then
		    # aarch64 vars starts as an empty file
		    dd if=/dev/zero of=${bios_code} bs=1M count=64
		    dd if=/dev/zero of=${bios_vars} bs=1M count=64
		    dd if=/usr/local/share/qemu/edk2-aarch64-code.fd of=${bios_code} conv=notrunc
		fi
		;;
	esac

	# Now make me a script
	img=${IMAGES}/${ma_combo}/linuxboot-${ma_combo}.img
	img2=${IMAGES}/${ma_combo}/linuxboot-${ma_combo}-raw
	img3=${IMAGES}/${ma_combo}/linuxboot-${ma_combo}-zfs.img
	out=${SCRIPTS}/${ma_combo}/linuxboot-test.sh
	out2=${SCRIPTS}/${ma_combo}/linuxboot-test-raw.sh
	out3=${SCRIPTS}/${ma_combo}/linuxboot-test-zfs.sh
	cd=${CACHE}/FreeBSD-13.1-RELEASE-arm64-aarch64-bootonly.iso
	mkdir -p ${SCRIPTS}/${ma_combo}
	case ${ma} in
	    amd64)
		cat > ${out} <<EOF
${qemu_bin}/qemu-system-x86_64 -nographic -m 512M \\
        -drive file=${img},if=none,id=drive0,cache=writeback,format=raw \\
        -device virtio-blk,drive=drive0,bootindex=0 \\
        -drive file=${bios_code},format=raw,if=pflash \\
        -drive file=${bios_vars},format=raw,if=pflash \\
        -monitor telnet::4444,server,nowait \\
        -serial stdio \$*
EOF
		;;
	    aarch64)
		# ESP version
		raw=${IMAGES}/${ma_combo}/freebsd-arm64-aarch64.img
		cat > ${out} <<EOF
${qemu_bin}/qemu-system-aarch64 -nographic -machine virt,gic-version=3 -m 512M -smp 4 \\
        -cpu cortex-a57 \\
	-drive file=${img},if=none,id=drive0,cache=writeback \\
	-device virtio-blk,drive=drive0,bootindex=0 \\
        -drive file=${raw},if=none,id=drive1,cache=writeback \\
	-device nvme,serial=fboot,drive=drive1,bootindex=1 \\
        -drive file=${bios_code},format=raw,if=pflash \\
        -drive file=${bios_vars},format=raw,if=pflash \\
        -monitor telnet::4444,server,nowait \\
        -serial stdio \$*
EOF
		# RAW version
		# Note: We have to use cortex-a57 for raw mode because the
		# kernel we use has issues with max.
		cat > ${out2} <<EOF
${qemu_bin}/qemu-system-aarch64 -m 1024 -cpu cortex-a57 -M virt \\
	-kernel ${img2}.kernel -initrd ${img2}.initrd \\
	-append "console=ttyAMA0" \\
        -drive file=${cd},if=none,id=drive0,cache=writeback,format=raw \\
        -device virtio-blk,drive=drive0,bootindex=0 \\
	-nographic -monitor telnet::4444,server,nowait \\
	-serial stdio \$*
EOF
		# ZFS version
		# Note: We have to use cortex-a57 for raw mode because the
		# kernel we use has issues with max.
		cat > ${out3} <<EOF
${qemu_bin}/qemu-system-aarch64 -nographic -machine virt,gic-version=3 -m 512M -smp 4 \\
        -cpu cortex-a57 \\
	-drive file=${img3},if=none,id=drive0,cache=writeback \\
	-device virtio-blk,drive=drive0,bootindex=0 \\
        -drive file=${bios_code},format=raw,if=pflash \\
        -drive file=${bios_vars},format=raw,if=pflash \\
        -monitor telnet::4444,server,nowait \\
        -serial stdio \$*
EOF
		;;
	esac
    done
}

make_freebsd_esps()
{
    # At the moment, we have just three (armv7 could also be here too, but we're not doing that)
#   for a in amd64:amd64 arm64:aarch64 riscv:riscv64; do
    for a in amd64:amd64 arm64:aarch64; do
	m=${a%%:*}
	ma=${a##*:}
	ma_combo="${m}"
	[ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
	dir=${TREES}/${ma_combo}/freebsd-esp
	dir2=${TREES}/${ma_combo}/test-stand
	rm -rf ${dir}
	mkdir -p ${dir}
	case ${ma} in
	    amd64) bin=x64 ;;
	    aarch64) bin=aa64 ;;
	esac
	mkdir -p ${dir}/efi/boot
	cp ${dir2}/boot/loader.efi ${dir}/efi/boot/boot${bin}.efi
    done
}

make_freebsd_images()
{
    # ESP variant: In this variant, riscv, amd64 and arm64 are created more or
    # less the same way. UEFI + ACPI implementations
#   for a in amd64:amd64 arm64:aarch64 riscv:riscv64; do
    for a in amd64:amd64 arm64:aarch64; do
	m=${a%%:*}
	ma=${a##*:}
	ma_combo="${m}"
	[ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
	src=${TREES}/${ma_combo}/freebsd-esp
	dir=${TREES}/${ma_combo}/freebsd
	dir2=${TREES}/${ma_combo}/test-stand
	esp=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.esp
	ufs=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.ufs
	img=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.img
	mkdir -p ${IMAGES}/${ma_combo}
	mkdir -p ${dir2}/etc
	cat > ${dir2}/etc/fstab <<EOF
/dev/ufs/root	/		ufs	rw	1	1
EOF
	${MAKEFS} -t msdos -o fat_type=32 -o sectors_per_cluster=1 \
	       -o volume_label=EFISYS -s100m ${esp} ${src}
	${MAKEFS} -t ffs -B little -s 200m -o label=root ${ufs} ${dir} ${dir2}
	${MKIMG} -s gpt -p efi:=${esp} -p freebsd-ufs:=${ufs} -o ${img}
	# rm -f ${esp} ${ufs}	# Don't need to keep this around
    done

    set -x

if false; then
    # BIOS i386
    a=i386:i386
    m=${a%%:*}
    ma=${a##*:}
    ma_combo="${m}"
    [ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
    dir=${TREES}/${ma_combo}/freebsd
    dir2=${TREES}/${ma_combo}/test-stand
    ufs=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.ufs
    img=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.img
    mkdir -p ${IMAGES}/${ma_combo}
    mkdir -p ${dir2}/etc
    cat > ${dir2}/etc/fstab <<EOF
/dev/ufs/root	/		ufs	rw	1	1
EOF
    ${MAKEFS} -t ffs -B little -s 200m \
	   -o label=root,version=2,bsize=32768,fsize=4096,density=16384 \
	   ${ufs} ${dir} ${dir2}
    ${MKIMG} -s gpt -b ${dir2}/boot/pmbr \
	  -p freebsd-boot:=${dir2}/boot/gptboot \
	  -p freebsd-ufs:=${ufs} \
	  -o ${img}
    rm -f ${src}/etc/fstab

    # PowerPC for 32-bit mac
    a=powerpc:powerpc
    m=${a%%:*}
    ma=${a##*:}
    ma_combo="${m}"
    [ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
    dir=${TREES}/${ma_combo}/freebsd
    dir2=${TREES}/${ma_combo}/test-stand
    ufs=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.ufs
    img=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.img
    mkdir -p ${IMAGES}/${ma_combo}
    mkdir -p ${dir2}/etc
    cat > ${dir2}/etc/fstab <<EOF
/dev/ufs/root	/		ufs	rw	1	1
EOF
    ${MAKEFS} -t ffs -B big -s 200m \
	   -o label=root,version=2,bsize=32768,fsize=4096,density=16384 \
	   ${ufs} ${dir} ${dir2}
    ${MKIMG} -a 1 -s apm \
        -p freebsd-boot:=${dir2}/boot/boot1.hfs \
        -p freebsd-ufs:=${ufs} \
        -o ${img}
fi

    set +x
}

make_freebsd_scripts()
{
    # At the moment, we have just two
    for a in amd64:amd64 arm64:aarch64; do
	m=${a%%:*}
	ma=${a##*:}
	ma_combo="${m}"
	[ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"

	# First off, update the edk firmware
	bios_code=${BIOS}/edk2-${ma_combo}-code.fd
	bios_vars=${BIOS}/edk2-${ma_combo}-vars.fd
	case ${ma} in
	    amd64)
		if [ ${bios_code} -ot /usr/local/share/qemu/edk2-x86_64-code.fd ]; then
		    cp /usr/local/share/qemu/edk2-x86_64-code.fd ${bios_code}
		    # vars file works on both 32 and 64 bit x86
#		    cp /usr/local/share/qemu/edk2-i386-vars.fd ${bios_vars}
		fi
		;;
	    aarch64)
		if [ ${bios_code} -ot /usr/local/share/qemu/edk2-aarch64-code.fd ]; then
		    # aarch64 vars starts as an empty file
		    dd if=/dev/zero of=${bios_code} bs=1M count=64
		    dd if=/dev/zero of=${bios_vars} bs=1M count=64
		    dd if=/usr/local/share/qemu/edk2-aarch64-code.fd of=${bios_code} conv=notrunc
		fi
		;;
	esac

	# Now make me a script
	img=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.img
	out=${SCRIPTS}/${ma_combo}/freebsd-test.sh
	mkdir -p ${SCRIPTS}/${ma_combo}
	case ${ma} in
	    amd64)
		cat > ${out} <<EOF
${qemu_bin}/qemu-system-x86_64 -nographic -m 512M \\
        -drive file=${img},if=none,id=drive0,cache=writeback,format=raw \\
        -device virtio-blk,drive=drive0,bootindex=0 \\
        -drive file=${bios_code},format=raw,if=pflash \\
        -drive file=${bios_vars},format=raw,if=pflash \\
        -monitor telnet::4444,server,nowait \\
        -serial stdio \$*
EOF
		;;
	    aarch64)
		# ESP version
		raw=${IMAGES}/${ma_combo}/nvme-test-empty.raw
		cat > ${out} <<EOF
${qemu_bin}/qemu-system-aarch64 -nographic -machine virt,gic-version=3 -m 512M \\
        -cpu cortex-a57 -drive file=${img},if=none,id=drive0,cache=writeback -smp 4 \\
        -device virtio-blk,drive=drive0,bootindex=0 \\
        -drive file=${bios_code},format=raw,if=pflash \\
        -drive file=${bios_vars},format=raw,if=pflash \\
        -drive file=${raw},if=none,id=drive1,cache=writeback,format=raw \\
	-device nvme,serial=deadbeef,drive=drive1 \\
        -monitor telnet::4444,server,nowait \\
        -serial stdio \$*
EOF
		;;
	esac
    done

if false; then
    set -x
    a=powerpc:powerpc
    m=${a%%:*}
    ma=${a##*:}
    ma_combo="${m}"
    [ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
    img=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.img
    out=${SCRIPTS}/${ma_combo}/freebsd-test.sh
    mkdir -p ${SCRIPTS}/${ma_combo}
    cat > ${out} <<EOF
${qemu_bin}/qemu-system-ppc -m 1g -M mac99,via=pmu \\
	-vga none -nographic \\
	-drive file=${img},if=virtio \\
	-prom-env "boot-device=/pci@f2000000/scsi/disk@0:,\\\\\\:tbxi" \\
        -monitor telnet::4444,server,nowait \\
        -serial stdio \$*
EOF

    set -x
    a=i386:i386
    m=${a%%:*}
    ma=${a##*:}
    ma_combo="${m}"
    [ "${m}" != "${ma}" ] && ma_combo="${m}-${ma}"
    img=${IMAGES}/${ma_combo}/freebsd-${ma_combo}.img
    out=${SCRIPTS}/${ma_combo}/freebsd-test.sh
    mkdir -p ${SCRIPTS}/${ma_combo}
    cat > ${out} <<EOF
${qemu_bin}/qemu-system-i386 -m 1g \\
	-vga none -nographic \\
	-drive file=${img},format=raw \\
	-nographic \\
        -monitor telnet::4444,server,nowait \\
        -serial stdio \$*
EOF
fi
}

# The smallest FAT32 filesystem is 33292 KB
espsize=33292

set -e
echo "src/stand test in ${STAND_ROOT}"
update_freebsd_img_cache
make_freebsd_minimal_trees
make_freebsd_test_trees
make_linux_initrds
make_linux_esps
make_freebsd_esps
make_freebsd_images
make_freebsd_scripts
make_linuxboot_images
make_linuxboot_scripts
