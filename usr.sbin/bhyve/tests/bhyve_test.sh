# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The FreeBSD Foundation

check_list_sanity()
{
	local file="$1"

	# Output must be non-empty
	atf_check test -s "${file}"

	# Entry names must not contain whitespace
	atf_check -s ignore grep -E "[[:space:]]+" "${file}"

	# Entries must not be duplicated
	sort "${file}" > "${file}.sorted"
	sort -u "${file}" > "${file}.sorted-unique"
	atf_check diff -u "${file}.sorted-unique" "${file}.sorted"
}

atf_test_case check_s_list
check_s_list_head()
{
	atf_set "descr" "Check PCI device listing"
	atf_set "require.progs" "bhyve"
}

check_s_list_body()
{
	atf_check -o save:devices bhyve -s help
	check_list_sanity devices

	# Check some essential devices available on all arches
	for device in hostbridge nvme virtio-net; do
		atf_check grep -Fxq "${device}" devices
	done
}

atf_test_case check_l_list
check_l_list_head()
{
	atf_set "descr" "Check LPC device listing"
	atf_set "require.arch" "amd64"
	atf_set "require.progs" "bhyve"
}

check_l_list_body()
{
	cat >expected <<EOF
bootrom
com1
com2
com3
com4
tpm
pc-testdev
EOF
	atf_check -o file:expected bhyve -l help
}

atf_test_case check_o_list
check_o_list_head()
{
	atf_set "descr" "Check config option listing"
	atf_set "require.progs" "bhyve"
}

check_o_list_body()
{
	atf_check -o save:options bhyve -o help
	check_list_sanity options

	# Options are sorted
	atf_check env LC_ALL=C sort -c options

	# Check the base schema, a linker-set extension,
	# and an option-set inheritance
	for option in memory.size pci.B.S.F.maxq pci.B.S.F.path; do
		atf_check grep -Fxq "${option}" options
	done

	# Arch-specific option
	case "$(uname -p)" in
	amd64)
		machdep_option="x86.strictmsr"
		;;
	aarch64|riscv64)
		machdep_option="console"
		;;
	*)
		atf_skip "unsupported architecture: $(uname -p)"
		;;
	esac

	atf_check grep -Fxq "${machdep_option}" options
}

atf_test_case check_config_dump_sanity
check_config_dump_sanity_head()
{
	atf_set "descr" "Check config.dump=1 sanity"
	atf_set "require.progs" "bhyve"
}

check_config_dump_sanity_body()
{
	atf_check -s exit:1 -o save:config \
		bhyve -o memory.size=123M -o config.dump=1 test-vm

	# Basic sanity
	for value in \
		"config.dump=1" \
		"memory.size=123M" \
		"name=test-vm"; do
		atf_check grep -Fxq "${value}" config
	done

	# Check that generated config could be fed back with -k
	atf_check -s exit:1 -o save:config.2 bhyve -k config

	atf_check -o save:config.sorted env LC_ALL=C sort config
	atf_check -o save:config.2.sorted env LC_ALL=C sort config.2

	atf_check diff -u config.sorted config.2.sorted
}

atf_init_test_cases()
{
	atf_add_test_case check_s_list
	atf_add_test_case check_l_list
	atf_add_test_case check_o_list
	atf_add_test_case check_config_dump_sanity
}
