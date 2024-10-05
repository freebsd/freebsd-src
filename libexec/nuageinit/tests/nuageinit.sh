#-
# Copyright (c) 2022 Baptiste Daroussin <bapt@FreeBSD.org>
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
	atf_check -s exit:1 -e match:"nuageinit: error parsing nocloud.*" /usr/libexec/nuageinit "${PWD}"/media/nuageinit/ nocloud
	printf "instance-id: iid-local01\nlocal-hostname: cloudimg\n" > "${PWD}"/media/nuageinit/meta-data
	atf_check -s exit:0 /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"hostname=\"cloudimg\"\n" cat etc/rc.conf.d/hostname
	cat > media/nuageinit/meta-data << EOF
instance-id: iid-local01
hostname: myhost
EOF
	atf_check -s exit:0 /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	atf_check -o inline:"hostname=\"myhost\"\n" cat etc/rc.conf.d/hostname
}

nocloud_userdata_script_body()
{
	mkdir -p media/nuageinit
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	printf "#!/bin/sh\necho yeah\n" > "${PWD}"/media/nuageinit/user-data
	chmod 755 "${PWD}"/media/nuageinit/user-data
	atf_check -s exit:0 -o inline:"yeah\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
}

nocloud_user_data_script_body()
{
	mkdir -p media/nuageinit
	printf "instance-id: iid-local01\n" > "${PWD}"/media/nuageinit/meta-data
	printf "#!/bin/sh\necho yeah\n" > "${PWD}"/media/nuageinit/user_data
	chmod 755 "${PWD}"/media/nuageinit/user_data
	atf_check -s exit:0 -o inline:"yeah\n" /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
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
    groups: users
    passwd: $6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit nocloud
	cat > expectedgroup << EOF
wheel:*:0:root,freebsd
users:*:1:foobar
admingroup:*:1001:root,sys
cloud-users:*:1002:
freebsd:*:1003:
foobar:*:1004:
EOF
	cat > expectedpasswd << 'EOF'
root:*:0:0::0:0:Charlie &:/root:/bin/sh
sys:*:1:0::0:0:Sys:/home/sys:/bin/sh
freebsd:freebsd:1001:1003::0:0:FreeBSD User:/home/freebsd:/bin/sh
foobar:$6$j212wezy$7H/1LT4f9/N3wpgNunhsIqtMj62OKiS3nyNwuizouQc3u7MbYCarYeAHWYPYb2FT.lbioDm2RrkJPb9BZMN1O/:1002:1004::0:0:Foo B. Bar:/home/foobar:/bin/sh
EOF
	sed -i "" "s/freebsd:.*:1001/freebsd:freebsd:1001/" "${PWD}"/etc/master.passwd
	atf_check -o file:expectedpasswd cat "${PWD}"/etc/master.passwd
	atf_check -o file:expectedgroup cat "${PWD}"/etc/group
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
ifconfig_${myiface}="inet 192.0.2.2/24"
ifconfig_${myiface}_ipv6="inet6 2001:db8::2/64"
ipv6_network_interfaces="${myiface}"
ipv6_default_interface="${myiface}"
EOF
	cat > routing << EOF
defaultrouter="192.0.2.1"
ipv6_defaultrouter="2001:db8::1"
ipv6_route_${myiface}="2001:db8::1 -prefixlen 128 -interface ${myiface}"
EOF
	atf_check -o file:network cat "${PWD}"/etc/rc.conf.d/network
	atf_check -o file:routing cat "${PWD}"/etc/rc.conf.d/routing
}

config2_body()
{
	mkdir -p media/nuageinit
	atf_check -s exit:1 -e match:"nuageinit: error parsing config-2 meta_data.json:.*" /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	printf "{}" > media/nuageinit/meta_data.json
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	cat > media/nuageinit/meta_data.json << EOF
{
    "hostname": "cloudimg"
}
EOF
	atf_check /usr/libexec/nuageinit "${PWD}"/media/nuageinit config-2
	atf_check -o inline:"hostname=\"cloudimg\"\n" cat etc/rc.conf.d/hostname
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
ifconfig_${myiface}="DHCP"
ifconfig_${myiface}_ipv6="inet6 2001:db8::3257:9652/64"
ipv6_network_interfaces="${myiface}"
ipv6_default_interface="${myiface}"
EOF
	cat > routing << EOF
ipv6_defaultrouter="fd00::1"
ipv6_route_${myiface}="fd00::1 -prefixlen 128 -interface ${myiface}"
ipv6_static_routes="${myiface}"
EOF
	atf_check -o file:network cat "${PWD}"/etc/rc.conf.d/network
	atf_check -o file:routing cat "${PWD}"/etc/rc.conf.d/routing
}

config2_network_static_v4_body()
{
	mkdir -p media/nuageinit
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
ifconfig_${myiface}="inet 10.184.0.244 netmask 255.255.240.0"
EOF
	cat > routing << EOF
route_cloudinit1_${myiface}="-net 10.0.0.0 11.0.0.1 255.0.0.0"
defaultrouter="23.253.157.1"
static_routes="cloudinit1_${myiface}"
EOF
	atf_check -o file:network cat "${PWD}"/etc/rc.conf.d/network
	atf_check -o file:routing cat "${PWD}"/etc/rc.conf.d/routing
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
}
