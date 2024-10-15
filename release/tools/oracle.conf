#!/bin/sh
# Set to a list of packages to install.
export VM_EXTRA_PACKAGES="
    comms/py-pyserial
    converters/base64
    devel/oci-cli
    devel/py-babel
    devel/py-iso8601
    devel/py-pbr
    devel/py-six
    ftp/curl
    lang/python
    lang/python3
    net/cloud-init
    net/py-eventlet
    net/py-netaddr
    net/py-netifaces
    net/py-oauth
    net/rsync
    panicmail
    security/ca_root_nss
    security/sudo
    sysutils/firstboot-freebsd-update
    sysutils/firstboot-pkgs
    sysutils/panicmail
    textproc/jq
    "

# Should be enough for base image, image can be resized in needed
export VMSIZE=6g

# Set to a list of third-party software to enable in rc.conf(5).
export VM_RC_LIST="
    cloudinit
    firstboot_pkgs
    firstboot_freebsd_update
    growfs
    ntpd
    ntpd_sync_on_start
    sshd
    zfs"

vm_extra_pre_umount() {
	cat <<-'EOF' >> ${DESTDIR}/etc/rc.conf
		dumpdev=AUTO
		sendmail_enable=NONE
EOF

	cat <<-'EOF' >> ${DESTDIR}/boot/loader.conf
		autoboot_delay="5"
		beastie_disable="YES"
		boot_serial="YES"
		loader_logo="none"
		cryptodev_load="YES"
		opensolaris_load="YES"
		xz_load="YES"
		zfs_load="YES"
EOF

	cat <<-'EOF' >> ${DESTDIR}/etc/ssh/sshd_config
		# S11 Configure the SSH service to prevent password-based login
		PermitRootLogin prohibit-password
		PasswordAuthentication no
		KbdInteractiveAuthentication no
		PermitEmptyPasswords no
		UseDNS no
EOF

	 # S14 Root user login must be disabled on serial-over-ssh console
	 pw -R ${DESTDIR} usermod root -w no
	 # Oracle requirements override the default FreeBSD cloud-init settings
	 cat <<-'EOF' >> ${DESTDIR}/usr/local/etc/cloud/cloud.cfg.d/98_oracle.cfg
		disable_root: true
		system_info:
		   distro: freebsd
		   default_user:
		     name: freebsd
		     lock_passwd: True
		     gecos: "Oracle Cloud Default User"
		     groups: [wheel]
		     sudo: ["ALL=(ALL) NOPASSWD:ALL"]
		     shell: /bin/sh
		   network:
		      renderers: ['freebsd']
EOF

	# Use Oracle Cloud Infrastructure NTP server
	sed -i '' -E -e 's/^pool.*iburst/server 169.254.169.254 iburst/' \
        ${DESTDIR}/etc/ntp.conf

	touch ${DESTDIR}/firstboot

	return 0
}
