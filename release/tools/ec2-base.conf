#!/bin/sh

. ${WORLDDIR}/release/tools/ec2.conf

# Packages to install into the image we're creating.  In addition to packages
# present on all EC2 AMIs, we install:
# * amazon-ssm-agent (not enabled by default, but some users need to use
# it on systems not connected to the internet),
# * ec2-scripts, which provides a range of EC2ification startup scripts,
# * firstboot-freebsd-update, to install security updates at first boot,
# * firstboot-pkgs, to install packages at first boot, and
# * isc-dhcp44-client, used for IPv6 network setup.
export VM_EXTRA_PACKAGES="${VM_EXTRA_PACKAGES} amazon-ssm-agent ec2-scripts \
    firstboot-freebsd-update firstboot-pkgs isc-dhcp44-client"

# Services to enable in rc.conf(5).
export VM_RC_LIST="${VM_RC_LIST} ec2_configinit ec2_ephemeral_swap \
    ec2_fetchkey ec2_loghostkey firstboot_freebsd_update firstboot_pkgs \
    growfs sshd"

vm_extra_pre_umount() {
	# The AWS CLI tools are generally useful, and small enough that they
	# will download quickly; but users will often override this setting
	# via EC2 user-data.
	echo 'firstboot_pkgs_list="devel/py-awscli"' >> ${DESTDIR}/etc/rc.conf

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
