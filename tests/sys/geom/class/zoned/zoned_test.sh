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

	zoned_backing_md
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

	zoned_backing_md
	atf_check gzoned create ${md}
	atf_check_equal "Host Managed" \
	    "$(zonectl -d /dev/${md}.zoned -c params | \
	    awk -F': ' '/Zone Mode/ {print $2}')"
}
is_zoned_cleanup()
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

	zoned_backing_md
	atf_check gzoned create -s 256m -c 1,3 ${md}
	atf_check_equal "2" "$(zone_count nonwp)"
}
conventional_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case conventional_single cleanup
conventional_single_head()
{
	atf_set "descr" "gzoned create -c accepts a single zone"
	atf_set "require.user" "root"
}
conventional_single_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m -c 2 ${md}
	atf_check_equal "1" "$(zone_count nonwp)"
	# The conventional zone is zone 2. At LBA 512m / 512 = 0x100000.
	atf_check_equal "0x100000" \
	    "$(zonectl -d /dev/${md}.zoned -c rz -o nonwp -P script | \
	    awk -F',' 'NR == 1 {gsub(/ /, "", $1); print $1}')"
}
conventional_single_cleanup()
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

	zoned_backing_md
	atf_check gzoned create -s 256m -c 0-2 ${md}
	atf_check_equal "3" "$(zone_count nonwp)"
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

	zoned_backing_md
	atf_check gzoned create -s 256m -c 0-3 ${md}
	atf_check -o match:"^4 zones," \
	    -o match:"Zone lengths and types are all the same" \
	    zonectl -d /dev/${md}.zoned -c rz -P summary
}
all_zones_same_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case conventional_ranges cleanup
conventional_ranges_head()
{
	atf_set "descr" "gzoned create -c accepts a list of zone ranges"
	atf_set "require.user" "root"
}
conventional_ranges_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m -c 0,2-3 ${md}
	atf_check_equal "3" "$(zone_count nonwp)"
	# Zone 1 alone stays sequential: the second conventional zone
	# reported must be zone 2, at LBA 512m / 512 = 0x100000.
	atf_check_equal "0x100000" \
	    "$(zonectl -d /dev/${md}.zoned -c rz -o nonwp -P script | \
	    awk -F',' 'NR == 2 {gsub(/ /, "", $1); print $1}')"
}
conventional_ranges_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case zone_size cleanup
zone_size_head()
{
	atf_set "descr" "gzoned create -s adjusts the zone size"
	atf_set "require.user" "root"
}
zone_size_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 128m ${md}
	# 1024m of zone space now holds eight 128m zones instead of four.
	atf_check -o match:"^8 zones," \
	    zonectl -d /dev/${md}.zoned -c rz -P summary
	# Zone 1 starts at LBA 128m / 512 = 0x40000.
	atf_check_equal "0x40000" "$(zone_start 1)"
}
zone_size_cleanup()
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

	zoned_backing_md
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

	zoned_backing_md
	atf_check gzoned create -s 256m -c 0 ${md}
	atf_check -s not-exit:0 -e ignore \
	    zonectl -d /dev/${md}.zoned -c rwp -l 0
}
reset_wp_conv_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case reset_wp_empty cleanup
reset_wp_empty_head()
{
	atf_set "descr" \
	    "gzoned accepts a write pointer reset of an already-empty zone"
	atf_set "require.user" "root"
}
reset_wp_empty_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m ${md}
	atf_check_equal "0" "$(zone_wp 0)"
	atf_check zonectl -d /dev/${md}.zoned -c rwp -l 0
	atf_check_equal "0" "$(zone_wp 0)"
	atf_check_equal "4" "$(zone_count empty)"
}
reset_wp_empty_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case reset_wp_finished cleanup
reset_wp_finished_head()
{
	atf_set "descr" "gzoned resets a finished zone back to empty"
	atf_set "require.user" "root"
}
reset_wp_finished_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m ${md}
	atf_check zonectl -d /dev/${md}.zoned -c finish -l 0
	atf_check_equal "1" "$(zone_count full)"

	atf_check zonectl -d /dev/${md}.zoned -c rwp -l 0
	atf_check_equal "0" "$(zone_count full)"
	atf_check_equal "0" "$(zone_wp 0)"
	atf_check_equal "4" "$(zone_count empty)"
}
reset_wp_finished_cleanup()
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

	zoned_backing_md
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

	zoned_backing_md
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

atf_test_case write_at_wp cleanup
write_at_wp_head()
{
	atf_set "descr" "gzoned accepts writes at the write pointer"
	atf_set "require.user" "root"
}
write_at_wp_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m ${md}
	atf_check -e ignore dd if=/dev/zero of=/dev/${md}.zoned bs=1m count=1
	atf_check_equal "0x800" "$(zone_wp 0)"
	# A second write at the advanced write pointer is accepted too.
	atf_check -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m oseek=1 count=1
	atf_check_equal "0x1000" "$(zone_wp 0)"
}
write_at_wp_cleanup()
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

	zoned_backing_md
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

	zoned_backing_md
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

atf_test_case write_conv_boundary cleanup
write_conv_boundary_head()
{
	atf_set "descr" \
	    "gzoned allows writes crossing two conventional zones"
	atf_set "require.user" "root"
}
write_conv_boundary_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m -c 0-1 ${md}
	# The same straddling write rejected in write_boundary is fine
	# when the zones on both sides are conventional.
	atf_check -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=768k oseek=341 count=1
}
write_conv_boundary_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case write_conv_anywhere cleanup
write_conv_anywhere_head()
{
	atf_set "descr" "gzoned conventional zones accept writes at any offset"
	atf_set "require.user" "root"
}
write_conv_anywhere_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m -c 0 ${md}
	# No write pointer: writing far into the zone and then again before
	# the previous write must both succeed, and the zone never opens.
	atf_check -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m oseek=10 count=1
	atf_check -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m oseek=2 count=1
	atf_check_equal "0" "$(zone_count imp_open)"
}
write_conv_anywhere_cleanup()
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

	zoned_backing_md
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

atf_test_case write_closed_zone cleanup
write_closed_zone_head()
{
	atf_set "descr" "gzoned writes to a closed zone implicitly reopen it"
	atf_set "require.user" "root"
}
write_closed_zone_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m ${md}
	atf_check -e ignore dd if=/dev/zero of=/dev/${md}.zoned bs=1m count=1
	atf_check zonectl -d /dev/${md}.zoned -c close -l 0
	atf_check_equal "1" "$(zone_count closed)"

	atf_check -e ignore \
	    dd if=/dev/zero of=/dev/${md}.zoned bs=1m oseek=1 count=1
	atf_check_equal "0" "$(zone_count closed)"
	atf_check_equal "1" "$(zone_count imp_open)"
}
write_closed_zone_cleanup()
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

	zoned_backing_md
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

atf_test_case clear_clears_metadata cleanup
clear_clears_metadata_head()
{
	atf_set "descr" "gzoned clear erases the on-disk metadata"
	atf_set "require.user" "root"
}
clear_clears_metadata_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m ${md}
	# The backing provider is held open for as long as the device exists,
	# so its metadata can only be cleared once the device is gone.
	atf_check -s not-exit:0 -e ignore gzoned clear /dev/${md}
	atf_check gzoned stop ${md}.zoned
	wait_dev_gone /dev/${md}.zoned
	atf_check gzoned clear /dev/${md}
	true > /dev/${md}
	sleep 1
	atf_check test ! -c /dev/${md}.zoned
}
clear_clears_metadata_cleanup()
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

	zoned_backing_md
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

atf_test_case max_open_limit cleanup
max_open_limit_head()
{
	atf_set "descr" "gzoned rejects explicit opens beyond the open limit"
	atf_set "require.user" "root"
}
max_open_limit_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m -m 2 ${md}
	atf_check zonectl -d /dev/${md}.zoned -c open -l 0
	atf_check zonectl -d /dev/${md}.zoned -c open -l 0x80000
	atf_check_equal "2" "$(zone_count exp_open)"

	# Explicitly open zones cannot be implicitly closed. A third open
	# exceeds the limit until one of them is closed.
	atf_check -s not-exit:0 -e ignore \
	    zonectl -d /dev/${md}.zoned -c open -l 0x100000
	atf_check zonectl -d /dev/${md}.zoned -c close -l 0
	atf_check zonectl -d /dev/${md}.zoned -c open -l 0x100000
	atf_check_equal "2" "$(zone_count exp_open)"
}
max_open_limit_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case stop_force cleanup
stop_force_head()
{
	atf_set "descr" "gzoned stop -f stops a busy device"
	atf_set "require.user" "root"
}
stop_force_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m ${md}

	# Keep the device open; a plain stop must fail, a forced one work.
	sleep 5 < /dev/${md}.zoned &
	spid=$!
	sleep 0.5
	atf_check -s not-exit:0 -e ignore gzoned stop ${md}.zoned
	atf_check gzoned stop -f ${md}.zoned
	wait_dev_gone /dev/${md}.zoned
	kill $spid 2>/dev/null
	wait $spid 2>/dev/null || true
}
stop_force_cleanup()
{
	gzoned_test_cleanup
}

atf_test_case create_on_zoned cleanup
create_on_zoned_head()
{
	atf_set "descr" \
	    "gzoned create on an already-zoned device fails gracefully"
	atf_set "require.user" "root"
}
create_on_zoned_body()
{
	gzoned_test_setup

	zoned_backing_md
	atf_check gzoned create -s 256m ${md}
	# The metadata of the second device cannot land in the sequential zones
	# of the first; creation must be refused and leave the first device
	# intact, with all zones still empty.
	atf_check -s not-exit:0 -e match:"host-managed" \
	    gzoned create -s 256m ${md}.zoned
	atf_check test -c /dev/${md}.zoned
	atf_check -o match:"^4 zones," \
	    zonectl -d /dev/${md}.zoned -c rz -P summary
	atf_check_equal "4" "$(zone_count empty)"
}
create_on_zoned_cleanup()
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

	zoned_backing_md
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

	zoned_backing_md
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
	atf_add_test_case create_on_zoned
	atf_add_test_case is_zoned
	atf_add_test_case conventional
	atf_add_test_case conventional_single
	atf_add_test_case conventional_range
	atf_add_test_case conventional_ranges
	atf_add_test_case all_zones_same
	atf_add_test_case zone_size
	atf_add_test_case reset_wp
	atf_add_test_case reset_wp_conv
	atf_add_test_case reset_wp_empty
	atf_add_test_case reset_wp_finished
	atf_add_test_case unrestricted_reads
	atf_add_test_case restricted_reads
	atf_add_test_case write_at_wp
	atf_add_test_case write_out_of_order
	atf_add_test_case write_boundary
	atf_add_test_case write_conv_boundary
	atf_add_test_case write_conv_anywhere
	atf_add_test_case write_full
	atf_add_test_case write_closed_zone
	atf_add_test_case persistence
	atf_add_test_case clear_clears_metadata
	atf_add_test_case max_open
	atf_add_test_case max_open_limit
	atf_add_test_case stop_force
	atf_add_test_case fault_injection
	atf_add_test_case delete_rejected
}
