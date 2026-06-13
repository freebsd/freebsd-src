# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 voidanix <voidanix@FreeBSD.org>
#

. $(atf_get_srcdir)/conf.sh

atf_test_case create cleanup
create_head()
{
	atf_set "descr" "Basic gzoned device creation"
	atf_set "require.user" "root"
}
create_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}
	atf_check test -c /dev/${md}.zoned
}
create_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case is_zoned cleanup
is_zoned_head()
{
	atf_set "descr" "gzoned creates a host-managed zoned device"
	atf_set "require.user" "root"
}
is_zoned_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create ${md}
	atf_check_equal "Host Managed" \
	    "$(zonectl -d /dev/${md}.zoned -c params | \
	    awk -F': ' '/Zone Mode/ {print $2}')"
}
is_zoned_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case sequential cleanup
sequential_head()
{
	atf_set "descr" "gzoned device has only sequential zones by default"
	atf_set "require.user" "root"
}
sequential_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}
	atf_check -o not-match:"Conventional" \
	    zonectl -d /dev/${md}.zoned -c rz
}
sequential_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case conventional cleanup
conventional_head()
{
	atf_set "descr" "gzoned device has a few conventional zones"
	atf_set "require.user" "root"
}
conventional_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m -c 1,3 ${md}
	atf_check_equal "2" \
	    "$(zonectl -d /dev/${md}.zoned -c rz | grep -c Conventional)"
}
conventional_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case conventional_range cleanup
conventional_range_head()
{
	atf_set "descr" "gzoned device has a range of conventional zones"
	atf_set "require.user" "root"
}
conventional_range_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m -c 0-2 ${md}
	atf_check_equal "3" \
	    "$(zonectl -d /dev/${md}.zoned -c rz | grep -c Conventional)"
}
conventional_range_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case all_zones_same cleanup
all_zones_same_head()
{
	atf_set "descr" "gzoned device has all zones of the same type"
	atf_set "require.user" "root"
}
all_zones_same_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m -c 0-3 ${md}
	atf_check -o match:"^4 zones," \
	    -o match:"Zone lengths and types are all the same" \
	    zonectl -d /dev/${md}.zoned -c rz -P summary
}
all_zones_same_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case report_filter cleanup
report_filter_head()
{
	atf_set "descr" "zonectl -o only reports zones matching the filter"
	atf_set "require.user" "root"
}
report_filter_body()
{
	gzoned_test_setup

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
	gzoned_test_cleanup
}

atf_test_case reset_wp cleanup
reset_wp_head()
{
	atf_set "descr" \
	    "gzoned write pointer can be reset inside a seq-write-req zone"
	atf_set "require.user" "root"
}
reset_wp_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}

	# Advance the write pointer of zone 0 by 1m (2048 512-byte LBAs).
	atf_check -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m count=1
	atf_check_equal "0x800" "$(zone_wp 0)"

	atf_check zonectl -d /dev/${md}.zoned -c rwp -l 0
	atf_check_equal "0" "$(zone_wp 0)"
}
reset_wp_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case reset_wp_conv cleanup
reset_wp_conv_head()
{
	atf_set "descr" \
	    "gzoned write pointer cannot be reset inside a conventional zone"
	atf_set "require.user" "root"
}
reset_wp_conv_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m -c 0 ${md}
	atf_check -s not-exit:0 -e ignore \
	    zonectl -d /dev/${md}.zoned -c rwp -l 0
}
reset_wp_conv_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case unrestricted_reads cleanup
unrestricted_reads_head()
{
	atf_set "descr" \
	    "gzoned device allows reads above the write pointer by default"
	atf_set "require.user" "root"
}
unrestricted_reads_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}
	atf_check -o match:"URSWRZ.*: Yes" \
	    zonectl -d /dev/${md}.zoned -c params
	# Reading an empty sequential zone is fine.
	atf_check -e ignore \
	    dd if=/dev/${md}.zoned of=/dev/null bs=512 count=1
}
unrestricted_reads_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case restricted_reads cleanup
restricted_reads_head()
{
	atf_set "descr" \
	    "gzoned -u device rejects reads beyond the write pointer"
	atf_set "require.user" "root"
}
restricted_reads_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m -u ${md}
	atf_check -o match:"URSWRZ.*: No" \
	    zonectl -d /dev/${md}.zoned -c params

	# Reading an empty sequential zone must fail...
	atf_check -s not-exit:0 -e ignore \
	    dd if=/dev/${md}.zoned of=/dev/null bs=512 count=1

	# ...until data has been written.
	atf_check -e ignore dd if=/dev/zero of=/dev/${md}.zoned bs=1m count=1
	atf_check -e ignore \
	    dd if=/dev/${md}.zoned of=/dev/null bs=1m count=1
	atf_check -s not-exit:0 -e ignore \
	    dd if=/dev/${md}.zoned of=/dev/null bs=1m count=2
}
restricted_reads_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case write_out_of_order cleanup
write_out_of_order_head()
{
	atf_set "descr" "gzoned rejects writes that skip the write pointer"
	atf_set "require.user" "root"
}
write_out_of_order_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}
	# Zone 0 write pointer is at LBA 0; writing at 1m shall fail.
	atf_check -s not-exit:0 -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m oseek=1 count=1
	atf_check_equal "0" "$(zone_wp 0)"
}
write_out_of_order_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case write_boundary cleanup
write_boundary_head()
{
	atf_set "descr" "gzoned rejects writes crossing a zone boundary"
	atf_set "require.user" "root"
}
write_boundary_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m -c 0 ${md}
	# One 768k write straddling the 256m mark (768k does not divide
	# 256m, so the request reaches the kernel as a single bio going
	# from the conventional zone 0 into the sequential zone 1).
	atf_check -s not-exit:0 -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=768k oseek=341 count=1
	# The preceding block lies within the conventional zone and is fine.
	atf_check -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=768k oseek=340 count=1
}
write_boundary_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case write_full cleanup
write_full_head()
{
	atf_set "descr" "gzoned rejects writes to a full zone"
	atf_set "require.user" "root"
}
write_full_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}
	atf_check zonectl -d /dev/${md}.zoned -c finish -l 0
	atf_check_equal "1" "$(zone_count full)"
	atf_check -s not-exit:0 -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m count=1
}
write_full_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case persistence cleanup
persistence_head()
{
	atf_set "descr" "gzoned zone state survives a stop and re-taste"
	atf_set "require.user" "root"
}
persistence_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}
	atf_check -e ignore dd if=/dev/zero of=/dev/${md}.zoned bs=1m count=1
	atf_check_equal "0x800" "$(zone_wp 0)"

	# Stopping will commit the zone state; re-tasting the backing provider
	# brings the device back with its write pointers restored.
	atf_check gzoned stop ${md}.zoned
	wait_dev_gone /dev/${md}.zoned
	true > /dev/${md}
	wait_dev /dev/${md}.zoned
	atf_check_equal "0x800" "$(zone_wp 0)"
}
persistence_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case max_open cleanup
max_open_head()
{
	atf_set "descr" "gzoned enforces the open-zone limit"
	atf_set "require.user" "root"
}
max_open_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m -m 1 ${md}
	atf_check -o match:"Open Sequential Write Required Zones: 1" \
	    zonectl -d /dev/${md}.zoned -c params

	# A write into a second zone implicitly closes the first.
	atf_check -e ignore dd if=/dev/zero of=/dev/${md}.zoned bs=1m count=1
	atf_check -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m oseek=256 count=1
	atf_check_equal "1" "$(zone_count closed)"
	atf_check_equal "1" "$(zone_count imp_open)"

	# Explicitly opening zone 0 closes the implicitly open zone 1; a second
	# explicit open will exceed the limit.
	atf_check zonectl -d /dev/${md}.zoned -c open -l 0
	atf_check_equal "1" "$(zone_count exp_open)"
	atf_check -s not-exit:0 -e ignore \
	    zonectl -d /dev/${md}.zoned -c open -l 0x80000
}
max_open_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case fault_injection cleanup
fault_injection_head()
{
	atf_set "descr" "gzoned faulted zones reject writes until cleared"
	atf_set "require.user" "root"
}
fault_injection_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}

	atf_check gzoned fault -z 1 -s ro ${md}.zoned
	atf_check_equal "1" "$(zone_count ro)"
	atf_check -s not-exit:0 -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m oseek=256 count=1

	atf_check gzoned fault -z 1 -s clear ${md}.zoned
	atf_check_equal "0" "$(zone_count ro)"
	atf_check -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m oseek=256 count=1

	atf_check gzoned fault -z 2 -s offline ${md}.zoned
	atf_check_equal "1" "$(zone_count offline)"
	atf_check gzoned fault -z 3 -s reset ${md}.zoned
	atf_check_equal "1" "$(zone_count reset)"
}
fault_injection_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case delete_rejected cleanup
delete_rejected_head()
{
	atf_set "descr" "gzoned rejects BIO_DELETE, hides TRIM support"
	atf_set "require.user" "root"
}
delete_rejected_body()
{
	gzoned_test_setup

	alloc_zoned_md
	atf_check gzoned create -s 256m ${md}
	atf_check_equal "No" \
	    "$(diskinfo -v /dev/${md}.zoned | \
	    awk '/TRIM\/UNMAP support/ {print $1}')"
	atf_check -s not-exit:0 -e ignore -o ignore \
	    trim -f /dev/${md}.zoned
}
delete_rejected_cleanup()
{
	gzoned_test_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case create
	atf_add_test_case is_zoned
	atf_add_test_case sequential
	atf_add_test_case conventional
	atf_add_test_case conventional_range
	atf_add_test_case all_zones_same
	atf_add_test_case report_filter
	atf_add_test_case reset_wp
	atf_add_test_case reset_wp_conv
	atf_add_test_case unrestricted_reads
	atf_add_test_case restricted_reads
	atf_add_test_case write_out_of_order
	atf_add_test_case write_boundary
	atf_add_test_case write_full
	atf_add_test_case persistence
	atf_add_test_case max_open
	atf_add_test_case fault_injection
	atf_add_test_case delete_rejected
}
