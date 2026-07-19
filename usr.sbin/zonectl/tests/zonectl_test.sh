# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 voidanix <voidanix@FreeBSD.org>
#

MD_DEV_FILE="md.dev"

alloc_zoned_md()
{
	atf_check truncate -s 1025m backing_file
	md=$(mdconfig -a -t vnode -f backing_file) || \
	    atf_fail "mdconfig -a failed"
	echo ${md} > $MD_DEV_FILE
}

zoned_md_cleanup()
{
	if [ -f "$MD_DEV_FILE" ]; then
		read md < $MD_DEV_FILE
		[ -c /dev/${md}.zoned ] && \
		    gzoned destroy ${md}.zoned 2>/dev/null
		mdconfig -d -u ${md} 2>/dev/null
	fi
	true
}

atf_test_case report_filter cleanup
report_filter_head()
{
	atf_set "descr" "zonectl -o only reports zones matching the filter"
	atf_set "require.user" "root"
}
report_filter_body()
{
	alloc_zoned_md
	atf_check gzoned create -s 256m -c 0-1 ${md}

	atf_check_equal "2" \
	    "$(zonectl -d /dev/${md}.zoned -c rz -o nonwp -P summary | \
	    awk '/zones,/ {print $1}')"
	atf_check_equal "2" \
	    "$(zonectl -d /dev/${md}.zoned -c rz -o empty -P summary | \
	    awk '/zones,/ {print $1}')"
	atf_check_equal "0" \
	    "$(zonectl -d /dev/${md}.zoned -c rz -o full -P summary | \
	    awk '/zones,/ {print $1}')"
}
report_filter_cleanup()
{
	zoned_md_cleanup
}

atf_test_case report_zones cleanup
report_zones_head()
{
	atf_set "descr" "zonectl -c rz reports every zone"
	atf_set "require.user" "root"
}
report_zones_body()
{
	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}
	atf_check -o match:"^4 zones," \
	    zonectl -d /dev/${md}.zoned -c rz -P summary
	atf_check_equal "4" \
	    "$(zonectl -d /dev/${md}.zoned -c rz -P script | \
	    awk 'END {print NR}')"
}
report_zones_cleanup()
{
	zoned_md_cleanup
}

atf_test_case report_starting_lba cleanup
report_starting_lba_head()
{
	atf_set "descr" "zonectl -c rz -l reports zones from a starting LBA"
	atf_set "require.user" "root"
}
report_starting_lba_body()
{
	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}
	# Only zones 2 and 3 lie at or past LBA 0x100000 (512m).
	atf_check_equal "2" \
	    "$(zonectl -d /dev/${md}.zoned -c rz -l 0x100000 -P summary | \
	    awk '/zones,/ {print $1}')"
	atf_check_equal "0x100000" \
	    "$(zonectl -d /dev/${md}.zoned -c rz -l 0x100000 -P script | \
	    awk -F',' 'NR == 1 {gsub(/ /, "", $1); print $1}')"
}
report_starting_lba_cleanup()
{
	zoned_md_cleanup
}

atf_test_case report_params cleanup
report_params_head()
{
	atf_set "descr" "zonectl -c params reports the device parameters"
	atf_set "require.user" "root"
}
report_params_body()
{
	alloc_zoned_md
	atf_check gzoned create -s 256m -m 3 ${md}
	atf_check -o match:"Zone Mode: Host Managed" \
	    -o match:"URSWRZ.*: Yes" \
	    -o match:"Open Sequential Write Required Zones: 3" \
	    zonectl -d /dev/${md}.zoned -c params
}
report_params_cleanup()
{
	zoned_md_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case report_filter
	atf_add_test_case report_zones
	atf_add_test_case report_starting_lba
	atf_add_test_case report_params
}
