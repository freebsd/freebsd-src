#
# Copyright (c) 2026 Dag-Erling Smørgrav
#
# SPDX-License-Identifier: BSD-2-Clause
#

pkg_prepare()
{
	: ${LOCALBASE:=$(sysctl -n user.localbase)}
	: ${LOCALBASE:=/usr/local}
	pkg7=$(command -pv pkg)
	pkg8=${LOCALBASE}/sbin/pkg
	atf_check test -x "${pkg7}"
	atf_check test -x "${pkg8}"
	atf_check test ! "${pkg7}" -ef "${pkg8}"
}

atf_test_case activation
activation_head()
{
	atf_set descr "Activation test without jail or root"
}
activation_body()
{
	pkg_prepare
	atf_check -e save:N7 "${pkg7}" -N
	atf_check -e save:N8 "${pkg8}" -N
	atf_check cmp N7 N8
	atf_check -o match:"packages installed" cat N7
	atf_check -o save:v7 "${pkg7}" --version
	atf_check -o save:v8 "${pkg8}" --version
	atf_check cmp v7 v8
	atf_check -o match:"^[0-9]+(\.[0-9]+)+$" cat v7
}

atf_test_case activation_jail cleanup
activation_jail_head()
{
	atf_set descr "Activation test with jail"
	atf_set require.user "root"
	atf_set require.files "/rescue/sh"
}
activation_jail_body()
{
	local jname="j$$"
	pkg_prepare

	# An empty jail is not bootstrapped
	atf_check mkdir dev bin
	atf_check install -m 0755 /rescue/sh bin/sh
	cat >jail.conf <<EOF
${jname} { mount.devfs; path="${PWD}"; persist; }
EOF
	atf_check jail -f jail.conf -qc "${jname}"
	atf_check -s exit:1 -e match:"pkg is not installed" \
	    "${pkg7}" -j "${jname}" -N

	# Add a fake pkg to our jail
	atf_check mkdir -p "${PWD}${LOCALBASE}/sbin"
	atf_check install -m 0755 - "${PWD}${LOCALBASE}/sbin/pkg" <<'EOF'
#!/bin/sh
while getopts ":Nr:" opt; do
    case $opt in
    N) echo "${0##*/}: 1 packages installed" >&2 ;;
    r) ;;
    '?') OPTIND=$((OPTIND - 1)); break ;;
    esac
done
shift $((OPTIND - 1))
for arg; do
    case $arg in
    --version) echo 1.2.3 ;;
    esac
done
EOF

	# Try again
	atf_check -e inline:"pkg: 1 packages installed\n" \
	    "${pkg7}" -j "${jname}" -N
	atf_check -o inline:"1.2.3\n" "${pkg7}" -j ${jname} --version
}
activation_jail_cleanup()
{
	if [ -f jail.conf ]; then
		jail -f jail.conf -qr '*' || true
	fi
}

atf_test_case activation_root
activation_root_head()
{
	atf_set descr "Activation test with root"
}
activation_root_body()
{
	pkg_prepare

	# An empty root is not bootstrapped
	atf_check -s exit:1 -e match:"pkg is not installed" \
	    "${pkg7}" -r "${PWD}" -N

	# Add a fake pkg to our root
	atf_check mkdir -p "${PWD}${LOCALBASE}/sbin"
	atf_check install -m 0755 - "${PWD}${LOCALBASE}/sbin/pkg" <<'EOF'
#!/bin/sh
while getopts ":Nr:" opt; do
    case $opt in
    N) echo "${0##*/}: 1 packages installed" >&2 ;;
    r) ;;
    '?') OPTIND=$((OPTIND - 1)); break ;;
    esac
done
shift $((OPTIND - 1))
for arg; do
    case $arg in
    --version) echo 1.2.3 ;;
    esac
done
EOF

	# Try again
	atf_check -e inline:"pkg: 1 packages installed\n" \
	    "${pkg7}" -r "${PWD}" -N
	atf_check -o inline:"1.2.3\n" "${pkg7}" -r "${PWD}" --version
}

atf_test_case add_jail cleanup
add_jail_head()
{
	atf_set descr "Install pkg in a jail"
	atf_set require.user "root"
}
add_jail_body()
{
	local jname="j$$"
	pkg_prepare
	atf_check -o match:"Creating package for pkg" "${pkg8}" create -l3 pkg
	atf_check mkdir -p dev usr/bin
	atf_check install -m 0755 /usr/bin/uname usr/bin
	cat >jail.conf <<EOF
${jname} { mount.devfs; path="${PWD}"; persist; }
EOF
	atf_check jail -f jail.conf -qc "${jname}"
	atf_check -s exit:1 -e match:"pkg is not installed" \
	    "${pkg7}" -j "${jname}" -N
	atf_check -e ignore -o match:"Installing pkg" \
	    "${pkg7}" -j "${jname}" add pkg-*pkg
	atf_check ln -f "${PWD}${LOCALBASE}/sbin/pkg-static" \
	    "${PWD}${LOCALBASE}/sbin/pkg"
	atf_check -e inline:"pkg: 1 packages installed\n" \
	    "${pkg7}" -j "${jname}" -N
}
add_jail_cleanup()
{
	if [ -f jail.conf ]; then
		jail -f jail.conf -qr '*' || true
	fi
}

atf_test_case add_root
add_root_head()
{
	atf_set descr "Install pkg in a root"
	atf_set require.user "root"
}
add_root_body()
{
	pkg_prepare
	atf_check -o match:"Creating package for pkg" "${pkg8}" create -l3 pkg
	atf_check -s exit:1 -e match:"pkg is not installed" \
	    "${pkg7}" -r "${PWD}" -N
	atf_check -e ignore -o match:"Installing pkg" \
	    "${pkg7}" -r "${PWD}" add pkg-*pkg
	atf_check -e inline:"pkg: 1 packages installed\n" \
	    "${pkg7}" -r "${PWD}" -N
}

atf_test_case config
config_head()
{
	atf_set descr "Verify that pkg passes on configuration options"
}
config_body()
{
	pkg_prepare
	atf_check -o inline:"FreeBSD:1:i386\n" \
	    -e match:"Major OS version upgrade detected" \
	    -e not-match:"Setting ABI requires setting OSVERSION" \
	    "${pkg7}" -oABI=FreeBSD:1:i386 -oOSVERSION=100000 config abi
	atf_check -o inline:"FreeBSD:1:i386\n" \
	    -e match:"Major OS version upgrade detected" \
	    -e not-match:"Setting ABI requires setting OSVERSION" \
	    "${pkg7}" -o ABI=FreeBSD:1:i386 -o OSVERSION=100000 config abi
	atf_check -o inline:"FreeBSD:1:i386\n" \
	    -e match:"Major OS version upgrade detected" \
	    -e not-match:"Setting ABI requires setting OSVERSION" \
	    "${pkg7}" --option=ABI=FreeBSD:1:i386 --option=OSVERSION=100000 config abi
	atf_check -o inline:"FreeBSD:1:i386\n" \
	    -e match:"Major OS version upgrade detected" \
	    -e not-match:"Setting ABI requires setting OSVERSION" \
	    "${pkg7}" --option ABI=FreeBSD:1:i386 --option OSVERSION=100000 config abi
}

atf_init_test_cases()
{
	atf_add_test_case activation
	atf_add_test_case activation_jail
	atf_add_test_case activation_root
	atf_add_test_case add_jail
	atf_add_test_case add_root
	atf_add_test_case config
}
