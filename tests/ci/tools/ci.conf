#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 The FreeBSD Foundation
#
# This software was developed by Cybermancer Infosec <bofh@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#
# Set to a list of third-party software to enable in rc.conf(5).
export VM_RC_LIST="auditd freebsdci"

if [ "${CITYPE}" != "smoke" ]; then
export VM_EXTRA_PACKAGES="coreutils devel/py-pytest gdb jq ksh93 net/py-dpkt net/scapy nist-kat nmap perl5 python python3 sudo tcptestsuite"

	if [ "${TARGET}" = "amd64" ]; then
		export VM_EXTRA_PACKAGES="${VM_EXTRA_PACKAGES} linux-c7-ltp"
	fi
fi

vm_extra_pre_umount() {
cat << EOF >> ${DESTDIR}/boot/loader.conf
autoboot_delay=1
beastie_disable="YES"
loader_logo="none"
console="comconsole,vidconsole"
net.fibs=3
net.inet.ip.fw.default_to_accept=1
mac_bsdextended_load="YES"
vfs.zfs.arc_max=4294967296
kern.vty=sc
EOF
cat << EOF >> ${DESTDIR}/etc/kyua/kyua.conf
test_suites.FreeBSD.ci = 'true'
test_suites.FreeBSD.fibs = '1 2'
test_suites.FreeBSD.allow_sysctl_side_effects = '1'
test_suites.FreeBSD.cam_test_device = '/dev/ada0'
test_suites.FreeBSD.disks = '/dev/vtbd2 /dev/vtbd3 /dev/vtbd4 /dev/vtbd5 /dev/vtbd6'
EOF
cat << EOF >> ${DESTDIR}/etc/rc.conf
kld_list=""				# Load modules needed by tests
kld_list="${kld_list} blake2"		# sys/opencrypto
kld_list="${kld_list} cryptodev"	# sys/opencrypto
kld_list="${kld_list} fusefs"		# sys/fs/fusefs
kld_list="${kld_list} ipsec"		# sys/netipsec
kld_list="${kld_list} mac_portacl"	# sys/mac/portacl
kld_list="${kld_list} mqueuefs"		# sys/kern/mqueue_test
kld_list="${kld_list} pfsync"		# sys/netpfil/pf (loads pf)
kld_list="${kld_list} pflog"		# sys/netpfil/pf
kld_list="${kld_list} ipl"		# sys/sbin/ipf (loads ipfilter)
kld_list="${kld_list} ipfw"		# sys/netpfil/ipfw (loads ipfw)
kld_list="${kld_list} ipfw_nat"		# sys/netpfil/ipfw (loads ipfw_nat)
kld_list="${kld_list} ipdivert"		# sys/netinet (loads ipdivert)
kld_list="${kld_list} dummynet"		# sys/netpfil/common
kld_list="${kld_list} carp"		# sys/netinet/carp
kld_list="${kld_list} if_stf"		# sys/net/if_stf
background_fsck="NO"
sendmail_enable="NONE"
cron_enable="NO"
syslogd_enable="NO"
newsyslog_enable="NO"
EOF
if [ "${CITYPE}" = "smoke" ]; then
cat << EOF >> ${DESTDIR}/etc/rc.conf
freebsdci_type="smoke"
EOF
fi
cat << EOF >> ${DESTDIR}/etc/sysctl.conf
kern.cryptodevallowsoft=1
kern.ipc.tls.enable=1
net.add_addr_allfibs=0
security.mac.bsdextended.enabled=0
vfs.aio.enable_unsafe=1
vfs.usermount=1
EOF
cat << EOF >> ${DESTDIR}/etc/fstab
fdesc                      /dev/fd fdescfs   rw      0       0
EOF
	mkdir -p ${DESTDIR}/usr/local/etc/rc.d
	echo $scriptdir
	cp -p ${scriptdir}/../../tests/ci/tools/freebsdci ${DESTDIR}/usr/local/etc/rc.d/
	touch ${DESTDIR}/firstboot

	return 0
}

vm_extra_pkg_rmcache() {
	if [ -e ${DESTDIR}/usr/local/sbin/pkg ]; then
		mount -t devfs devfs ${DESTDIR}/dev
		chroot ${DESTDIR} ${EMULATOR} env ASSUME_ALWAYS_YES=yes \
			/usr/local/sbin/pkg clean -a
		umount_loop ${DESTDIR}/dev
	fi

	return 0
}
