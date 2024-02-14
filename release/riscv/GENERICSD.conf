#!/bin/sh

EMBEDDED_TARGET_ARCH="riscv64"
EMBEDDED_TARGET="riscv"
EMBEDDEDBUILD=1
FAT_SIZE="54m -b 8m"
FAT_TYPE="16"
IMAGE_SIZE="6144M"
KERNEL="GENERIC"
MD_ARGS="-x 63 -y 255"
PART_SCHEME="GPT"
EFIPART_SUFFIX=p3
ROOTFSPART_SUFFIX=p4
export BOARDNAME="GENERICSD"

arm_create_partitions() {
    # Create two partitions for firmware, preceding EFI and ROOTFS:
    #  1. u-boot SPL
    #  2. u-boot loader
    #
    #  The exact partition types can be rewritten by the user, but they should
    #  be reserved now.

    chroot ${CHROOTDIR} gpart add -t hifive-fsbl -l spl -a 512k -b 2m -s 2m ${mddev}
    chroot ${CHROOTDIR} gpart add -t hifive-bbl -l uboot -a 512k -b 4m -s 4m ${mddev}
}
