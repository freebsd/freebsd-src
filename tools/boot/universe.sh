#!/bin/sh

#
# Full list of all arches we don't build.
#
#	powerpc/powerpcspe arm/armv6
#
# This script is expected to be run in stand (though you could run it anywhere
# in the tree). It does a full clean build. For stand you can do all the archs in
# about a minute or two on a fast machine. It's also possible that you need a full
# make universe for this to work completely.
#
# Output is put into _.boot.$TARGET_ARCH.log in sys.boot.
#

die()
{
    echo $*
    exit 1
}

dobuild()
{
    local ta=$1
    local lf=$2
    local opt=$3

    echo -n "Building $ta ${opt} ... "
    objdir=$(make buildenv TARGET_ARCH=$ta BUILDENV_SHELL="make -V .OBJDIR" | tail -1)
    case ${objdir} in
	/*) ;;
	make*) echo Error message from make: $objdir
	       continue ;;
	*) die Crazy object dir: $objdir ;;
    esac
    rm -rf ${objdir}
    if ! make buildenv TARGET_ARCH=$ta BUILDENV_SHELL="make clean cleandepend cleandir obj depend"  \
	 > $lf 2>&1; then
	echo "Fail (cleanup)"
	continue
    fi
    if ! make buildenv TARGET_ARCH=$ta BUILDENV_SHELL="make ${opt} -j 40 all"  \
	 >> $lf 2>&1; then
	echo "Fail (build)"
	continue
    fi
    echo "Success"
}

top=$(make -V SRCTOP)
cd $top/stand

# Build without forth
for i in \
	arm64/aarch64 \
	amd64/amd64 \
	i386/i386 \
	; do
    ta=${i##*/}
    dobuild $ta _.boot.${ta}.no_forth.log "WITHOUT_FORTH=yes"
done

# Build without GELI
for i in \
	arm64/aarch64 \
	amd64/amd64 \
	i386/i386 \
	; do
    ta=${i##*/}
    dobuild $ta _.boot.${ta}.no_geli.log "WITHOUT_LOADER_GEIL=yes"
done

# Default build for a almost all architectures
for i in \
	amd64/amd64 \
	arm/armv7 \
	arm64/aarch64 \
	i386/i386 \
	powerpc/powerpc \
	powerpc/powerpc64 \
	powerpc/powerpc64le \
	riscv/riscv64 \
	; do
    ta=${i##*/}
    dobuild $ta _.boot.${ta}.log ""
done

# Build w/o ZFS
for i in \
	arm64/aarch64 \
	amd64/amd64 \
	i386/i386 \
	; do
    ta=${i##*/}
    dobuild $ta _.boot.${ta}.no_zfs.log "MK_LOADER_ZFS=no"
done

# Build w/ LOADER_BIOS_TEXTONLY
for i in \
	amd64/amd64 \
	i386/i386 \
	; do
    ta=${i##*/}
    dobuild $ta _.boot.${ta}.no_zfs.log "MK_LOADER_BIOS_TEXTONLY=yes"
done
