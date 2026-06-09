#-
# Copyright (c) 2022-2025 Baptiste Daroussin <bapt@FreeBSD.org>
# Copyright (c) 2025 Jesús Daniel Colmenares Oviedo <dtxdf@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

export NUAGE_FAKE_ROOTDIR="$PWD"

atf_test_case args
atf_test_case nocloud
atf_test_case nocloud_userdata_script
atf_test_case nocloud_user_data_script
atf_test_case nocloud_userdata_cloudconfig_users
atf_test_case nocloud_network
atf_test_case config2
atf_test_case config2_pubkeys
atf_test_case config2_pubkeys_user_data
atf_test_case config2_pubkeys_meta_data
atf_test_case config2_network
atf_test_case config2_network_static_v4
atf_test_case config2_network_dns
atf_test_case config2_ssh_keys
atf_test_case nocloud_userdata_cloudconfig_ssh_pwauth
atf_test_case nocloud_userdata_cloudconfig_chpasswd
atf_test_case nocloud_userdata_cloudconfig_chpasswd_list_string
atf_test_case nocloud_userdata_cloudconfig_chpasswd_list_list
atf_test_case config2_userdata_runcmd
atf_test_case config2_userdata_packages
atf_test_case config2_userdata_update_packages
atf_test_case config2_userdata_upgrade_packages
atf_test_case config2_userdata_shebang
atf_test_case config2_userdata_ssh_deletekeys
atf_test_case config2_userdata_disable_root
atf_test_case config2_userdata_bootcmd
atf_test_case config2_userdata_manage_etc_hosts
atf_test_case config2_userdata_mounts
atf_test_case config2_userdata_resolv_conf
atf_test_case config2_userdata_keyboard
atf_test_case config2_userdata_ssh_authkey_fingerprints
atf_test_case config2_userdata_ntp
atf_test_case config2_userdata_ca_certs
atf_test_case config2_userdata_multipart
atf_test_case config2_userdata_power_state
atf_test_case config2_userdata_locale
atf_test_case config2_userdata_fqdn_and_hostname
atf_test_case config2_userdata_write_files
atf_test_case config2_userdata_encode_base64
atf_test_case config2_userdata_final_message
atf_test_case config2_userdata_phone_home

setup_test_adduser()
{
	here=$(pwd)
	export NUAGE_FAKE_ROOTDIR=$(pwd)
	mkdir -p etc/ssh
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/csh
sys:*:1:0::0:0:Sys:/home/sys:/bin/csh
EOF
	pwd_mkdb -d etc ${here}/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
}

args_body()
{
	atf_check -s exit:1 -e inline:"Usage: /usr/libexec/nuageinit <cloud-init-directory> (<config-2> | <nocloud>)\n" /usr/libexec/nuageinit
	atf_check -s exit:1 -e inline:"Usage: /usr/libexec/nuageinit <cloud-init-directory> (<config-2> | <nocloud>)\n" /usr/libexec/nuageinit bla
	atf_check -s exit:1 -e inline:"Usage: /usr/libexec/nuageinit <cloud-init-directory> (<config-2> | <nocloud>)\n" /usr/libexec/nuageinit bla meh plop
	atf_check -s exit:1 -e inline:"nuageinit: Unknown cloud init type: meh\n" /usr/libexec/nuageinit bla meh
}

nocloud_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	atf_check -s exit:1 -e match:"nuageinit: error parsing nocloud.*" /usr/libexec/nuageinit "${PWD}"/media/nuageinit/ nocloud
	printf "instance-id: iid-local01\nlocal-hostname: cloudimg\n" > "${PWD}"/media/nuageinit/meta-data
	atf_check -s exit:0 /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"hostname='cloudimg'\n" cat etc/rc.conf.d/hostname
	cat > media/nuageinit/meta-data << EOF
instance-id: iid-local01
hostname: myhost
EOF
	atf_check -s exit:0 /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"hostname='myhost'\n" cat etc/rc.conf.d/hostname
}

nocloud_userdata_script_body()
{
	mkdir -p media/nuageinit
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	# ensure this is an invalid when parsed with the yaml parser
	printf "#!/bin/sh\n: ${test:-yes}\necho $test\n" > "${PWD}"/media/nuageinit/user-data
	chmod 644 "${PWD}"/media/nuageinit/user-data
	atf_check -s exit:0 /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check test -x var/cache/nuageinit/user_data
	atf_check -o inline:"#!/bin/sh\n: ${test:-yes}\necho $test\n" cat var/cache/nuageinit/user_data
}

nocloud_user_data_script_body()
{
	mkdir -p media/nuageinit
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	printf "#!/bin/sh\necho yeah\n" > "${PWD}"/media/nuageinit/user_data
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check -s exit:0 /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"#!/bin/sh\necho yeah\n" cat var/cache/nuageinit/user_data
}

nocloud_userdata_cloudconfig_users_head()
{
	atf_set "require.user" root
}
nocloud_userdata_cloudconfig_users_body()
{
	mkdir -p media/nuageinit
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	mkdir -p etc
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
EOF
	pwd_mkdb -d etc "${PWD}"/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
groups:
  - admingroup: [root,sys]
  - cloud-users
users:
  - default
  - name: foobar
    gecos: Foo B. Bar
    primary_group: foobar
    sudo: ALL=(ALL) NOPASSWD:ALL
    doas: permit persist %u as root
    groups: users
    passwd: $6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/
  - name: bla
    sudo:
    - "ALL=(ALL) NOPASSWD:/usr/sbin/pw"
    - "ALL=(ALL) ALL"
    doas:
    - "deny %u as foobar"
    - "permit persist %u as root cmd whoami"
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	cat > expectedgroup << EOF
wheel:*:0:root,freebsd
users:*:1:foobar
admingroup:*:1001:root,sys
cloud-users:*:1002:
freebsd:*:1003:
foobar:*:1004:
bla:*:1005:
EOF
	cat > expectedpasswd << 'EOF'
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
freebsd:freebsd:1001:1003::0:0:FreeBSD User:/home/freebsd:/bin/sh
foobar:$6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/:1002:1004::0:0:Foo B. Bar:/home/foobar:/bin/sh
bla::1003:1005::0:0:bla User:/home/bla:/bin/sh
EOF
	sed -i "" "s/freebsd:.*:1001/freebsd:freebsd:1001/" "${PWD}"/etc/master.passwd
	atf_check -o file:expectedpasswd cat "${PWD}"/etc/master.passwd
	atf_check -o file:expectedgroup cat "${PWD}"/etc/group
	localbase=`sysctl -ni user.localbase 2> /dev/null`
	if [ -z "${localbase}" ]; then
		# fallback
		localbase="/usr/local"
	fi
	atf_check -o inline:"foobar ALL=(ALL) NOPASSWD:ALL\nbla ALL=(ALL) NOPASSWD:/usr/sbin/pw\nbla ALL=(ALL) ALL\n" cat "${PWD}/${localbase}/etc/sudoers.d/90-nuageinit-users"
	atf_check -o inline:"permit persist foobar as root\ndeny bla as foobar\npermit persist bla as root cmd whoami\n" cat "${PWD}/${localbase}/etc/doas.conf"
}

nocloud_network_head()
{
	atf_set "require.user" root
}
nocloud_network_body()
{
	mkdir -p media/nuageinit
	mkdir -p etc
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
EOF
	pwd_mkdb -d etc "${PWD}"/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	mynetworks=$(ifconfig -l ether)
	if [ -z "$mynetworks" ]; then
		atf_skip "a network interface is needed"
	fi
	set -- $mynetworks
	myiface=$1
	myaddr=$(ifconfig $myiface ether | awk '/ether/ { print $2 }')
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	cat > media/nuageinit/user-data << EOF
#cloud-config
network:
  version: 2
  ethernets:
    # opaque ID for physical interfaces, only referred to by other stanzas
    id0:
      match:
        macaddress: "$myaddr"
      addresses:
        - 192.0.2.2/24
        - 2001:db8::2/64
      gateway4: 192.0.2.1
      gateway6: 2001:db8::1
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	cat > network << EOF
ifconfig_${myiface}='inet 192.0.2.2/24'
ifconfig_${myiface}_ipv6='inet6 2001:db8::2/64'
ipv6_network_interfaces='${myiface}'
ipv6_default_interface='${myiface}'
EOF
	cat > routing << EOF
defaultrouter='192.0.2.1'
ipv6_defaultrouter='2001:db8::1'
ipv6_route_${myiface}='2001:db8::1 -prefixlen 128 -interface ${myiface}'
EOF
	atf_check -o file:network cat "${PWD}"/etc/rc.conf.d/network
	atf_check -o file:routing cat "${PWD}"/etc/rc.conf.d/routing
}

config2_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	atf_check -s exit:1 -e match:"nuageinit: error parsing config-2 meta_data.json:.*" /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	printf "{}" > media/nuageinit/meta_data.json
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	cat > media/nuageinit/meta_data.json << EOF
{
    "hostname": "cloudimg"
}
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"hostname='cloudimg'\n" cat etc/rc.conf.d/hostname
}

config2_pubkeys_head()
{
	atf_set "require.user" root
}
config2_pubkeys_body()
{
	mkdir -p media/nuageinit
	touch media/nuageinit/meta_data.json
	cat > media/nuageinit/user-data << EOF
#cloud-config
ssh_authorized_keys:
  - "ssh-rsa AAAAB3NzaC1y...== Generated by Nova"
EOF
	mkdir -p etc
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
EOF
	pwd_mkdb -d etc "${PWD}"/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"ssh-rsa AAAAB3NzaC1y...== Generated by Nova\n" cat home/freebsd/.ssh/authorized_keys
}

config2_pubkeys_user_data_head()
{
	atf_set "require.user" root
}
config2_pubkeys_user_data_body()
{
	mkdir -p media/nuageinit
	touch media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data << EOF
#cloud-config
ssh_authorized_keys:
  - "ssh-rsa AAAAB3NzaC1y...== Generated by Nova"
EOF
	mkdir -p etc
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
EOF
	pwd_mkdb -d etc "${PWD}"/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"ssh-rsa AAAAB3NzaC1y...== Generated by Nova\n" cat home/freebsd/.ssh/authorized_keys
}

config2_pubkeys_meta_data_body()
{
	here=$(pwd)
	export NUAGE_FAKE_ROOTDIR=$(pwd)
	if [ $(id -u) -ne 0 ]; then
		atf_skip "root required"
	fi
	mkdir -p media/nuageinit
	cat > media/nuageinit/meta_data.json << EOF
{
    "uuid": "uuid_for_this_instance",
    "admin_pass": "a_generated_password",
    "public_keys": {
        "tdb": "ssh-ed25519 my_key_id tdb@host"
    },
    "keys": [
        {
            "name": "tdb",
            "type": "ssh",
            "data": "ssh-ed25519 my_key_id tdb@host"
        }
    ],
    "hostname": "freebsd-14-test.novalocal",
    "name": "freebsd-14-test",
    "launch_index": 0,
    "availability_zone": "nova",
    "random_seed": "long_random_seed",
    "project_id": "my_project_id",
    "devices": [],
    "dedicated_cpus": []
}
EOF
	mkdir -p etc
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/csh
sys:*:1:0::0:0:Sys:/home/sys:/bin/csh
EOF
	pwd_mkdb -d etc ${here}/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	atf_check /usr/libexec/nuageinit ${here}/media/nuageinit config-2
	atf_check -o inline:"ssh-ed25519 my_key_id tdb@host\n" cat home/freebsd/.ssh/authorized_keys
}

config2_network_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	mynetworks=$(ifconfig -l ether)
	if [ -z "$mynetworks" ]; then
		atf_skip "a network interface is needed"
	fi
	set -- $mynetworks
	myiface=$1
	myaddr=$(ifconfig $myiface ether | awk '/ether/ { print $2 }')
cat > media/nuageinit/network_data.json << EOF
{
    "links": [
        {
            "ethernet_mac_address": "$myaddr",
            "id": "iface0",
            "mtu": null
        }
    ],
    "networks": [
        {
            "id": "network0",
            "link": "iface0",
            "type": "ipv4_dhcp"
        },
        { // IPv6
            "id": "private-ipv4",
            "type": "ipv6",
            "link": "iface0",
            // supports condensed IPv6 with CIDR netmask
            "ip_address": "2001:db8::3257:9652/64",
            "gateway": "fd00::1",
            "routes": [
                {
                    "network": "::",
                    "netmask": "::",
                    "gateway": "fd00::1"
                },
                {
                    "network": "::",
                    "netmask": "ffff:ffff:ffff::",
                    "gateway": "fd00::1:1"
                }
            ],
            "network_id": "da5bb487-5193-4a65-a3df-4a0055a8c0d8"
        }
    ]
}
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	cat > network << EOF
ifconfig_${myiface}='DHCP'
ifconfig_${myiface}_ipv6='inet6 2001:db8::3257:9652/64'
ipv6_network_interfaces='${myiface}'
ipv6_default_interface='${myiface}'
EOF
	cat > routing << EOF
ipv6_defaultrouter='fd00::1'
ipv6_route_${myiface}='fd00::1 -prefixlen 128 -interface ${myiface}'
ipv6_static_routes='${myiface}'
EOF
	atf_check -o file:network cat "${PWD}"/etc/rc.conf.d/network
	atf_check -o file:routing cat "${PWD}"/etc/rc.conf.d/routing
}

config2_network_static_v4_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	mynetworks=$(ifconfig -l ether)
	if [ -z "$mynetworks" ]; then
		atf_skip "a network interface is needed"
	fi
	set -- $mynetworks
	myiface=$1
	myaddr=$(ifconfig $myiface ether | awk '/ether/ { print $2 }')
cat > media/nuageinit/network_data.json << EOF
{
    "links": [
        {
            "ethernet_mac_address": "$myaddr",
            "id": "iface0",
            "mtu": null
        }
    ],
    "networks": [
        {
            "id": "network0",
            "link": "iface0",
            "type": "ipv4",
            "ip_address": "10.184.0.244",
            "netmask": "255.255.240.0",
            "routes": [
                {
                    "network": "10.0.0.0",
                    "netmask": "255.0.0.0",
                    "gateway": "11.0.0.1"
                },
                {
                    "network": "0.0.0.0",
                    "netmask": "0.0.0.0",
                    "gateway": "23.253.157.1"
                }
            ]
        }
    ]
}
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	cat > network << EOF
ifconfig_${myiface}='inet 10.184.0.244 netmask 255.255.240.0'
EOF
	cat > routing << EOF
route_cloudinit1_${myiface}='-net 10.0.0.0 11.0.0.1 255.0.0.0'
defaultrouter='23.253.157.1'
static_routes='cloudinit1_${myiface}'
EOF
	atf_check -o file:network cat "${PWD}"/etc/rc.conf.d/network
	atf_check -o file:routing cat "${PWD}"/etc/rc.conf.d/routing
}

config2_network_dns_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	mynetworks=$(ifconfig -l ether)
	if [ -z "$mynetworks" ]; then
		atf_skip "a network interface is needed"
	fi
	set -- $mynetworks
	myiface=$1
	myaddr=$(ifconfig $myiface ether | awk '/ether/ { print $2 }')
cat > media/nuageinit/network_data.json << EOF
{
    "links": [
        {
            "ethernet_mac_address": "$myaddr",
            "id": "iface0",
            "mtu": null
        }
    ],
    "networks": [
        {
            "id": "network0",
            "link": "iface0",
            "type": "ipv4_dhcp"
        }
    ],
    "services": [
        {
            "type": "dns",
            "address": "9.9.9.9"
        },
        {
            "type": "dns",
            "address": "149.112.112.112"
        }
    ]
}
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"name_servers='9.9.9.9 149.112.112.112'\n" \
		cat "${PWD}"/etc/resolvconf.conf
}

config2_ssh_keys_head()
{
	atf_set "require.user" root
}
config2_ssh_keys_body()
{
	here=$(pwd)
	export NUAGE_FAKE_ROOTDIR=$(pwd)
	mkdir -p media/nuageinit
	touch media/nuageinit/meta_data.json
	cat > media/nuageinit/user-data << EOF
#cloud-config
ssh_keys:
  rsa_private: |
    -----BEGIN RSA PRIVATE KEY-----
    MIIBxwIBAAJhAKD0YSHy73nUgysO13XsJmd4fHiFyQ+00R7VVu2iV9Qco
    ...
    -----END RSA PRIVATE KEY-----
  rsa_public: ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAGEAoPRhIfLvedSDKw7Xd ...
  ed25519_private: |
    -----BEGIN OPENSSH PRIVATE KEY-----
    blabla
    ...
    -----END OPENSSH PRIVATE KEY-----
  ed25519_public: ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIK+MH4E8KO32N5CXRvXVqvyZVl0+6ue4DobdhU0FqFd+
EOF
	mkdir -p etc/ssh
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/csh
sys:*:1:0::0:0:Sys:/home/sys:/bin/csh
EOF
	pwd_mkdb -d etc ${here}/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	_expected="-----BEGIN RSA PRIVATE KEY-----
MIIBxwIBAAJhAKD0YSHy73nUgysO13XsJmd4fHiFyQ+00R7VVu2iV9Qco
...
-----END RSA PRIVATE KEY-----

"
	atf_check -o inline:"${_expected}" cat ${PWD}/etc/ssh/ssh_host_rsa_key
	_expected="ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAGEAoPRhIfLvedSDKw7Xd ...\n"
	atf_check -o inline:"${_expected}" cat ${PWD}/etc/ssh/ssh_host_rsa_key.pub
	_expected="-----BEGIN OPENSSH PRIVATE KEY-----
blabla
...
-----END OPENSSH PRIVATE KEY-----

"
	atf_check -o inline:"${_expected}" cat ${PWD}/etc/ssh/ssh_host_ed25519_key
	_expected="ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIK+MH4E8KO32N5CXRvXVqvyZVl0+6ue4DobdhU0FqFd+\n"
	atf_check -o inline:"${_expected}" cat ${PWD}/etc/ssh/ssh_host_ed25519_key.pub
}


nocloud_userdata_cloudconfig_ssh_pwauth_head()
{
	atf_set "require.user" root
}
nocloud_userdata_cloudconfig_ssh_pwauth_body()
{
	mkdir -p etc
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
EOF
	pwd_mkdb -d etc "${PWD}"/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	mkdir -p media/nuageinit
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
ssh_pwauth: true
EOF
	mkdir -p etc/ssh/
	touch etc/ssh/sshd_config

	atf_check -o empty -e empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"PasswordAuthentication yes\n" cat etc/ssh/sshd_config

	# Same value we don't touch anything
	printf "   PasswordAuthentication yes # I want password\n" > etc/ssh/sshd_config
	atf_check -o empty -e empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"   PasswordAuthentication yes # I want password\n" cat etc/ssh/sshd_config

	printf "   PasswordAuthentication no # Should change\n" > etc/ssh/sshd_config
	atf_check -o empty -e empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"PasswordAuthentication yes\n" cat etc/ssh/sshd_config

	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
ssh_pwauth: false
EOF

	printf "   PasswordAuthentication no # no passwords\n" > etc/ssh/sshd_config
	atf_check -o empty -e empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"   PasswordAuthentication no # no passwords\n" cat etc/ssh/sshd_config

	printf "   PasswordAuthentication yes # Should change\n" > etc/ssh/sshd_config
	atf_check -o empty -e empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"PasswordAuthentication no\n" cat etc/ssh/sshd_config
}

nocloud_userdata_cloudconfig_chpasswd_head()
{
	atf_set "require.user" root
}
nocloud_userdata_cloudconfig_chpasswd_body()
{
	mkdir -p etc
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
user:*:1:0::0:0:Sys:/home/sys:/bin/sh
EOF
	pwd_mkdb -d etc "${PWD}"/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	mkdir -p media/nuageinit
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
chpasswd:
  expire: true
  users:
  - { user: "sys", password: RANDOM }
EOF

	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o empty -e inline:"nuageinit: Invalid entry for chpasswd.users: missing 'name'\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	# nothing modified
	atf_check -o inline:"sys:*:1:0::0:0:Sys:/home/sys:/bin/sh\n" pw -R $(pwd) usershow sys

	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
chpasswd:
  expire: true
  users:
  - { name: "sys", pwd: RANDOM }
EOF
	atf_check -o empty -e inline:"nuageinit: Invalid entry for chpasswd.users: missing 'password'\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	# nothing modified
	atf_check -o inline:"sys:*:1:0::0:0:Sys:/home/sys:/bin/sh\n" pw -R $(pwd) usershow sys

	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
chpasswd:
  expire: false
  users:
  - { name: "sys", password: RANDOM }
EOF
	# not empty because the password is printed to stdout
	atf_check -o empty -e empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	atf_check -o match:'sys:\$.*:1:0::0:0:Sys:/home/sys:/bin/sh$' pw -R $(pwd) usershow sys

	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
chpasswd:
  expire: true
  users:
  - { name: "sys", password: RANDOM }
EOF
	# not empty because the password is printed to stdout
	atf_check -o empty -e empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	atf_check -o match:'sys:\$.*:1:0::1:0:Sys:/home/sys:/bin/sh$' pw -R $(pwd) usershow sys

	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
chpasswd:
  expire: true
  users:
  - { name: "user", password: "$6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/" }
EOF
	# not empty because the password is printed to stdout
	atf_check -o empty -e empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	atf_check -o inline:'user:$6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/:1:0::1:0:Sys:/home/sys:/bin/sh\n' pw -R $(pwd) usershow user
}


nocloud_userdata_cloudconfig_chpasswd_list_string_head()
{
	atf_set "require.user" root
}
nocloud_userdata_cloudconfig_chpasswd_list_string_body()
{
	mkdir -p etc
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
user:*:1:0::0:0:Sys:/home/sys:/bin/sh
EOF
	pwd_mkdb -d etc "${PWD}"/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	mkdir -p media/nuageinit
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
chpasswd:
  expire: true
  list: |
     sys:RANDOM
EOF

	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o empty -e inline:"nuageinit: chpasswd.list is deprecated consider using chpasswd.users\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	atf_check -o match:'sys:\$.*:1:0::1:0:Sys:/home/sys:/bin/sh$' pw -R $(pwd) usershow sys

	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
chpasswd:
  expire: false
  list: |
     sys:plop
     user:$6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/
     root:R
EOF

	atf_check -o empty -e ignore /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	atf_check -o match:'sys:\$.*:1:0::0:0:Sys:/home/sys:/bin/sh$' pw -R $(pwd) usershow sys
	atf_check -o inline:'user:$6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/:1:0::0:0:Sys:/home/sys:/bin/sh\n' pw -R $(pwd) usershow user
	atf_check -o match:'root:\$.*:0:0::0:0:Charlie &:/root:/bin/sh$' pw -R $(pwd) usershow root
}

nocloud_userdata_cloudconfig_chpasswd_list_list_head()
{
	atf_set "require.user" root
}
nocloud_userdata_cloudconfig_chpasswd_list_list_body()
{
	mkdir -p etc
	cat > etc/master.passwd << EOF
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
user:*:1:0::0:0:Sys:/home/sys:/bin/sh
EOF
	pwd_mkdb -d etc "${PWD}"/etc/master.passwd
	cat > etc/group << EOF
wheel:*:0:root
users:*:1:
EOF
	mkdir -p media/nuageinit
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
chpasswd:
  expire: true
  list:
  - sys:RANDOM
EOF

	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o empty -e inline:"nuageinit: chpasswd.list is deprecated consider using chpasswd.users\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	atf_check -o match:'sys:\$.*:1:0::1:0:Sys:/home/sys:/bin/sh$' pw -R $(pwd) usershow sys

	cat > media/nuageinit/user-data << 'EOF'
#cloud-config
chpasswd:
  expire: false
  list:
  - sys:plop
  - user:$6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/
  - root:R
EOF

	atf_check -o empty -e ignore /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	atf_check -o match:'sys:\$.*:1:0::0:0:Sys:/home/sys:/bin/sh$' pw -R $(pwd) usershow sys
	atf_check -o inline:'user:$6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/:1:0::0:0:Sys:/home/sys:/bin/sh\n' pw -R $(pwd) usershow user
	atf_check -o match:'root:\$.*:0:0::0:0:Charlie &:/root:/bin/sh$' pw -R $(pwd) usershow root
}

config2_userdata_runcmd_head()
{
	atf_set "require.user" root
}
config2_userdata_runcmd_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
runcmd:
EOF
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
runcmd:
  - plop
EOF
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check -s exit:0 /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	test -f var/cache/nuageinit/runcmds || atf_fail "File not created"
	test -x var/cache/nuageinit/runcmds || atf_fail "Missing execution permission"
	atf_check -o inline:"#!/bin/sh\nplop\n" cat var/cache/nuageinit/runcmds

	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
runcmd:
  - echo "yeah!"
  - uname -s
EOF
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"#!/bin/sh\necho \"yeah!\"\nuname -s\n" cat var/cache/nuageinit/runcmds
}

config2_userdata_packages_head()
{
	atf_set "require.user" root
}

config2_userdata_packages_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	export NUAGE_RUN_TESTS=1
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
packages:
EOF
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
packages:
  - yeah/plop
EOF
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check -s exit:0 -o inline:"pkg install -y 'yeah/plop'\npkg info -q 'yeah/plop'\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet

	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
packages:
  - curl
EOF
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check -o inline:"pkg install -y 'curl'\npkg info -q 'curl'\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet

	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
packages:
  - curl
  - meh: bla
EOF
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check -o inline:"pkg install -y 'curl'\npkg info -q 'curl'\n" -e inline:"nuageinit: Invalid type: table for packages entry number 2\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
}

config2_userdata_update_packages_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	export NUAGE_RUN_TESTS=1
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
package_update: true
EOF
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check -o inline:"env ASSUME_ALWAYS_YES=yes pkg update\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
}

config2_userdata_upgrade_packages_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	export NUAGE_RUN_TESTS=1
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
package_upgrade: true
EOF
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check -o inline:"env ASSUME_ALWAYS_YES=yes pkg upgrade\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
}

config2_userdata_shebang_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#!/we/dont/care
anything
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	test -f var/cache/nuageinit/user_data || atf_fail "File not created"
	test -x var/cache/nuageinit/user_data || atf_fail "Missing execution permission"
	atf_check -o inline:"#!/we/dont/care\nanything\n" cat var/cache/nuageinit/user_data
	cat > media/nuageinit/user_data <<EOF
/we/dont/care
EOF
	rm var/cache/nuageinit/user_data
	if [ -f var/cache/nuageinit/user_data ]; then
		atf_fail "File should not have been created"
	fi
}

config2_userdata_write_files_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
write_files:
- content: "plop"
  path: /file1
- path: /emptyfile
- content: !!binary |
    YmxhCg==
  path: /file_base64
  encoding: b64
  permissions: '0755'
  owner: nobody
- content: "bob"
  path: "/foo"
  defer: true
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"plop" cat file1
	atf_check -o inline:"" cat emptyfile
	atf_check -o inline:"bla\n" cat file_base64
	test -f foo && atf_fail "foo creation should have been deferred"
	atf_check -o match:"^-rwxr-xr-x.*nobody" ls -l file_base64
	rm file1 emptyfile file_base64
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	test -f file1 -o -f emptyfile -o -f file_base64 && atf_fail "defer not working properly"
	atf_check -o inline:"bob" cat foo
}

config2_userdata_ssh_deletekeys_head()
{
	atf_set "require.user" root
}
config2_userdata_ssh_deletekeys_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
ssh_deletekeys: true
EOF
	mkdir -p etc/ssh
	touch etc/ssh/ssh_host_rsa_key
	touch etc/ssh/ssh_host_rsa_key.pub
	touch etc/ssh/ssh_host_ed25519_key
	touch etc/ssh/ssh_host_ed25519_key.pub
	touch etc/ssh/ssh_host_ecdsa_key
	touch etc/ssh/ssh_host_ecdsa_key.pub
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	test -f etc/ssh/ssh_host_rsa_key && atf_fail "ssh_host_rsa_key not deleted"
	test -f etc/ssh/ssh_host_rsa_key.pub && atf_fail "ssh_host_rsa_key.pub not deleted"
	test -f etc/ssh/ssh_host_ed25519_key && atf_fail "ssh_host_ed25519_key not deleted"
	test -f etc/ssh/ssh_host_ed25519_key.pub && atf_fail "ssh_host_ed25519_key.pub not deleted"
	test -f etc/ssh/ssh_host_ecdsa_key && atf_fail "ssh_host_ecdsa_key not deleted"
	test -f etc/ssh/ssh_host_ecdsa_key.pub && atf_fail "ssh_host_ecdsa_key.pub not deleted"
	true
}

config2_userdata_disable_root_head()
{
	atf_set "require.user" root
}
config2_userdata_disable_root_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
disable_root: true
EOF
	mkdir -p etc/ssh
	touch etc/ssh/sshd_config
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"PermitRootLogin no\n" cat etc/ssh/sshd_config
	cat > media/nuageinit/user_data <<EOF
#cloud-config
disable_root: true
disable_root_opts: "without-password"
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"PermitRootLogin without-password\n" cat etc/ssh/sshd_config
	cat > media/nuageinit/user_data <<EOF
#cloud-config
disable_root: true
disable_root_opts:
  - "prohibit-password"
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"PermitRootLogin prohibit-password\n" cat etc/ssh/sshd_config
	cat > media/nuageinit/user_data <<EOF
#cloud-config
disable_root: false
EOF
	echo "PermitRootLogin yes" > etc/ssh/sshd_config
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"PermitRootLogin yes\n" cat etc/ssh/sshd_config
}

config2_userdata_bootcmd_head()
{
	atf_set "require.user" root
}
config2_userdata_bootcmd_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
bootcmd:
  - kldload if_bridge
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	test -f var/cache/nuageinit/bootcmds || atf_fail "bootcmds file not created"
	atf_check -o inline:"#!/bin/sh\nkldload if_bridge\n" cat var/cache/nuageinit/bootcmds
	cat > media/nuageinit/user_data <<EOF
#cloud-config
bootcmd:
  - sysctl net.inet.ip.forwarding=1
  - kldload if_bridge
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"#!/bin/sh\nsysctl net.inet.ip.forwarding=1\nkldload if_bridge\n" cat var/cache/nuageinit/bootcmds
	# Test 3: empty list (clean up from previous tests first)
	rm -f var/cache/nuageinit/bootcmds
	cat > media/nuageinit/user_data <<EOF
#cloud-config
bootcmd: []
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	test -f var/cache/nuageinit/bootcmds && atf_fail "bootcmds should not have been created for empty list"
	true
}

config2_userdata_manage_etc_hosts_head()
{
	atf_set "require.user" root
}
config2_userdata_manage_etc_hosts_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	# Test 1: manage_etc_hosts adds hostname when /etc/hosts does not exist
	cat > media/nuageinit/user_data <<EOF
#cloud-config
hostname: mycloud
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"::1\t\tlocalhost mycloud\n127.0.0.1\t\tlocalhost mycloud\n" cat etc/hosts
	# Test 2: manage_etc_hosts appends hostname to existing localhost lines
	cat > etc/hosts <<EOF
::1		localhost
127.0.0.1		localhost
EOF
	cat > media/nuageinit/user_data <<EOF
#cloud-config
hostname: myvm
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"::1\t\tlocalhost myvm\n127.0.0.1\t\tlocalhost myvm\n" cat etc/hosts
	# Test 3: hostname already present in /etc/hosts, no change
	cat > etc/hosts <<EOF
::1		localhost myvm
127.0.0.1		localhost myvm
EOF
	cat > media/nuageinit/user_data <<EOF
#cloud-config
hostname: myvm
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"::1\t\tlocalhost myvm\n127.0.0.1\t\tlocalhost myvm\n" cat etc/hosts
	# Test 4: manage_etc_hosts: false disables the behaviour
	cat > etc/hosts <<EOF
::1		localhost
127.0.0.1		localhost
EOF
	cat > media/nuageinit/user_data <<EOF
#cloud-config
hostname: nope
manage_etc_hosts: false
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"::1\t\tlocalhost\n127.0.0.1\t\tlocalhost\n" cat etc/hosts
}

config2_userdata_mounts_head()
{
	atf_set "require.user" root
}
config2_userdata_mounts_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
mounts:
  - [ /dev/ada1p1, /mnt/data, ufs, rw, 0, 2 ]
  - device: tmpfs
    mountpoint: /mnt/tmp
    fstype: tmpfs
    options: "size=256M"
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o match:"/dev/ada1p1.*/mnt/data.*ufs.*rw.*0.*2" cat etc/fstab
	atf_check -o match:"tmpfs.*/mnt/tmp.*tmpfs.*size=256M.*0.*0" cat etc/fstab
	test -d mnt/data || atf_fail "/mnt/data directory not created"
	test -d mnt/tmp || atf_fail "/mnt/tmp directory not created"
	true
}

config2_userdata_resolv_conf_head()
{
	atf_set "require.user" root
}
config2_userdata_resolv_conf_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
resolv_conf:
  nameservers:
    - 9.9.9.9
    - 149.112.112.112
  searchdomains:
    - example.com
    - test.local
  domain: mydomain.local
  options:
    timeout: "1"
    attempts: "2"
  sortlist:
    - 192.168.1.0/255.255.255.0
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o match:"domain mydomain.local" cat etc/resolv.conf
	atf_check -o match:"search example.com test.local" cat etc/resolv.conf
	atf_check -o match:"sortlist 192.168.1.0/255.255.255.0" cat etc/resolv.conf
	atf_check -o match:"nameserver 9.9.9.9" cat etc/resolv.conf
	atf_check -o match:"nameserver 149.112.112.112" cat etc/resolv.conf
	atf_check -o match:"options.*timeout:1" cat etc/resolv.conf
	atf_check -o match:"options.*attempts:2" cat etc/resolv.conf
	true
}

config2_userdata_keyboard_head()
{
	atf_set "require.user" root
}
config2_userdata_keyboard_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
keyboard:
  layout: fr
  variant: acc
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"keymap='fr.acc'\n" cat etc/rc.conf.d/keymap
	true
}

config2_userdata_ssh_authkey_fingerprints_head()
{
	atf_set "require.user" root
}
config2_userdata_ssh_authkey_fingerprints_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	mkdir -p etc/ssh
	ssh-keygen -t ed25519 -f etc/ssh/ssh_host_ed25519_key -N "" -q 2>/dev/null
	cat > media/nuageinit/user_data <<EOF
#cloud-config
ssh_authkey_fingerprints: true
EOF
	atf_check -s exit:0 -e match:"SHA256:" /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	true
}

config2_userdata_ntp_head()
{
	atf_set "require.user" root
}
config2_userdata_ntp_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
ntp:
  servers:
    - 192.168.1.1
    - 10.0.0.1
  pools:
    - 0.pool.ntp.org
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	atf_check -o match:"server 192.168.1.1 iburst" cat etc/ntp.conf
	atf_check -o match:"server 10.0.0.1 iburst" cat etc/ntp.conf
	atf_check -o match:"pool 0.pool.ntp.org iburst" cat etc/ntp.conf
	atf_check -o match:"leapfile /var/db/ntpd.leap-seconds.list" cat etc/ntp.conf
	true
}

config2_userdata_ca_certs_head()
{
	atf_set "require.user" root
}
config2_userdata_ca_certs_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<'EOF'
#cloud-config
ca_certs:
  trusted:
    - |
      -----BEGIN CERTIFICATE-----
      dGVzdGNlcnQx
      -----END CERTIFICATE-----
    - |
      -----BEGIN CERTIFICATE-----
      dGVzdGNlcnQy
      -----END CERTIFICATE-----
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o match:"dGVzdGNlcnQx" cat usr/share/certs/trusted/nuageinit-1.pem
	atf_check -o match:"dGVzdGNlcnQy" cat usr/share/certs/trusted/nuageinit-2.pem
	true
}

config2_userdata_multipart_head()
{
	atf_set "require.user" root
}
config2_userdata_multipart_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<'EOF'
Content-Type: multipart/mixed; boundary="==BOUNDARY=="

--==BOUNDARY==
Content-Type: text/cloud-config; charset="us-ascii"

#cloud-config
hostname: multipart-host

--==BOUNDARY==
Content-Type: text/x-shellscript

#!/bin/sh
echo "multipart script executed"

--==BOUNDARY==--
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"hostname='multipart-host'\n" cat etc/rc.conf.d/hostname
	atf_check -o inline:"#!/bin/sh\necho \"multipart script executed\"\n" cat var/cache/nuageinit/multipart_script
	test -x var/cache/nuageinit/multipart_script || atf_fail "multipart_script not executable"
	true
}

config2_userdata_power_state_head()
{
	atf_set "require.user" root
}
config2_userdata_power_state_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	export NUAGE_RUN_TESTS=1
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
power_state:
  delay: "+5"
  mode: reboot
  message: "Rebooting after configuration is complete"
  timeout: 30
  condition: true
EOF
	atf_check -o inline:"shutdown -r '+5' 'Rebooting after configuration is complete'\n" \
	    /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
	true
}

config2_userdata_locale_head()
{
	atf_set "require.user" root
}
config2_userdata_locale_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
locale: fr_FR.UTF-8
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"export LANG='fr_FR.UTF-8'\n" cat etc/profile

	cat > media/nuageinit/user_data <<EOF
#cloud-config
locale:
  LANG: de_DE.UTF-8
  LC_ALL: de_DE.UTF-8
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o match:"export LANG='de_DE.UTF-8'" cat etc/profile
	atf_check -o match:"export LC_ALL='de_DE.UTF-8'" cat etc/profile
	true
}

config2_userdata_fqdn_and_hostname_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data <<EOF
#cloud-config
fqdn: host.domain.tld
hostname: host
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"hostname='host.domain.tld'\n" cat ${PWD}/etc/rc.conf.d/hostname
	cat > media/nuageinit/user_data <<EOF
#cloud-config
hostname: host
EOF
	atf_check -o empty /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"hostname='host'\n" cat ${PWD}/etc/rc.conf.d/hostname
}

config2_userdata_encode_base64_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	atf_check -o inline:"dGVzdA==\n" \
	    /usr/libexec/flua -e "print(require('nuage').encode_base64('test'))"
	atf_check -o inline:"dA==\n" \
	    /usr/libexec/flua -e "print(require('nuage').encode_base64('t'))"
	atf_check -o inline:"dGU=\n" \
	    /usr/libexec/flua -e "print(require('nuage').encode_base64('te'))"
	# Roundtrip test
	atf_check -o inline:"hello world\n" \
	    /usr/libexec/flua -e "print(require('nuage').decode_base64(require('nuage').encode_base64('hello world')))"
	# Empty input
	atf_check -o inline:"\n" \
	    /usr/libexec/flua -e "print(require('nuage').encode_base64(''))"
}

config2_userdata_final_message_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	export NUAGE_RUN_TESTS=1
	printf "{}" > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
final_message: "System ready after $UPTIME seconds"
EOF
	atf_check -e match:"System ready after [0-9]+ seconds" \
	    /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
}

config2_userdata_phone_home_body()
{
	mkdir -p media/nuageinit
	setup_test_adduser
	export NUAGE_RUN_TESTS=1
	printf '{"hostname": "myhost", "uuid": "abc-123", "public_keys": ["ssh-rsa AAAAB...", "ssh-ed25519 AAAAC..."]}' > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
phone_home:
  url: "http://example.com/endpoint"
  post:
    - hostname
    - instance_id
  tries: 1
EOF
	atf_check -o match:"fetch -q -o /dev/null --post-data 'hostname=myhost&instance_id=abc-123' 'http://example.com/endpoint'" \
	    /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet

	# Test "all" post
	printf '{"hostname": "myhost"}' > media/nuageinit/meta_data.json
	cat > media/nuageinit/user_data << 'EOF'
#cloud-config
phone_home:
  url: "http://example.com/endpoint"
  post: all
  tries: 1
EOF
	atf_check -o match:"fetch -q -o /dev/null --post-data 'hostname=myhost&fqdn=myhost' 'http://example.com/endpoint'" \
	    /usr/libexec/nuageinit "${PWD}"/media/nuageinit postnet
}

atf_init_test_cases()
{
	atf_add_test_case args
	atf_add_test_case nocloud
	atf_add_test_case nocloud_userdata_script
	atf_add_test_case nocloud_user_data_script
	atf_add_test_case nocloud_userdata_cloudconfig_users
	atf_add_test_case nocloud_network
	atf_add_test_case config2
	atf_add_test_case config2_pubkeys
	atf_add_test_case config2_pubkeys_user_data
	atf_add_test_case config2_pubkeys_meta_data
	atf_add_test_case config2_network
	atf_add_test_case config2_network_static_v4
	atf_add_test_case config2_network_dns
	atf_add_test_case config2_ssh_keys
	atf_add_test_case nocloud_userdata_cloudconfig_ssh_pwauth
	atf_add_test_case nocloud_userdata_cloudconfig_chpasswd
	atf_add_test_case nocloud_userdata_cloudconfig_chpasswd_list_string
	atf_add_test_case nocloud_userdata_cloudconfig_chpasswd_list_list
	atf_add_test_case config2_userdata_runcmd
	atf_add_test_case config2_userdata_packages
	atf_add_test_case config2_userdata_update_packages
	atf_add_test_case config2_userdata_upgrade_packages
	atf_add_test_case config2_userdata_shebang
	atf_add_test_case config2_userdata_ssh_deletekeys
	atf_add_test_case config2_userdata_disable_root
	atf_add_test_case config2_userdata_bootcmd
	atf_add_test_case config2_userdata_manage_etc_hosts
	atf_add_test_case config2_userdata_mounts
	atf_add_test_case config2_userdata_resolv_conf
	atf_add_test_case config2_userdata_keyboard
	atf_add_test_case config2_userdata_ssh_authkey_fingerprints
	atf_add_test_case config2_userdata_ntp
	atf_add_test_case config2_userdata_ca_certs
	atf_add_test_case config2_userdata_multipart
	atf_add_test_case config2_userdata_power_state
	atf_add_test_case config2_userdata_locale
	atf_add_test_case config2_userdata_fqdn_and_hostname
	atf_add_test_case config2_userdata_write_files
	atf_add_test_case config2_userdata_encode_base64
	atf_add_test_case config2_userdata_final_message
	atf_add_test_case config2_userdata_phone_home
}
