#!/bin/sh

. ${WORLDDIR}/release/tools/ec2.conf

# Build with a 4.9 GB partition; the growfs rc.d script will expand
# the partition to fill the root disk after the EC2 instance is launched.
# Note that if this is set to <N>G, we will end up with an <N+1> GB disk
# image since VMSIZE is the size of the filesystem partition, not the disk
# which it resides within.  (This overrides the default in ec2.conf.)
export VMSIZE=5000m

# Flags to installworld/kernel: We don't want debug symbols (kernel or
# userland), 32-bit libraries, tests, or the debugger.
export INSTALLOPTS="WITHOUT_DEBUG_FILES=YES WITHOUT_KERNEL_SYMBOLS=YES \
    WITHOUT_LIB32=YES WITHOUT_TESTS=YES WITHOUT_LLDB=YES"

# Packages to install into the image we're creating.  In addition to packages
# present on all EC2 AMIs, we install:
# * ec2-scripts, which provides a range of EC2ification startup scripts,
# * firstboot-freebsd-update, to install security updates at first boot,
# * firstboot-pkgs, to install packages at first boot, and
# * isc-dhcp44-client, used for IPv6 network setup.
export VM_EXTRA_PACKAGES="${VM_EXTRA_PACKAGES} ec2-scripts \
    firstboot-freebsd-update firstboot-pkgs isc-dhcp44-client"

# Services to enable in rc.conf(5).
export VM_RC_LIST="${VM_RC_LIST} ec2_configinit ec2_ephemeral_swap \
    ec2_fetchkey ec2_loghostkey firstboot_freebsd_update firstboot_pkgs \
    growfs sshd"

vm_extra_pre_umount() {
	# Any EC2 ephemeral disks seen when the system first boots will
	# be "new" disks; there is no "previous boot" when they might have
	# been seen and used already.
	touch ${DESTDIR}/var/db/ec2_ephemeral_diskseen

	# Configuration common to all EC2 AMIs
	ec2_common

	# Standard FreeBSD network configuration
	ec2_base_networking

	return 0
}
