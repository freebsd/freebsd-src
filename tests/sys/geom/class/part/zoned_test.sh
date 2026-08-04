#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 voidanix <voidanix@FreeBSD.org>
#

class=part
. $(atf_get_srcdir)/../geom_subr.sh

zoned_part_test_setup()
{
	geom_atf_test_setup
	if ! error_message=$(geom_load_class_if_needed zoned); then
		atf_skip "$error_message"
	fi
}

# Zones given in the optional first argument are conventional.
alloc_zoned_md()
{
	local conv=$1

	atf_check truncate -s 1025m backing_file
	attach_md md -t vnode -f backing_file
	atf_check gzoned create -s 256m ${conv:+-c ${conv}} ${md}
}

# Print number of zones matching a report option on the given device.
zone_count()
{
	zonectl -d $1 -c rz -o $2 -P summary | awk '/zones,/ {print $1}'
}

# Wait until gpart knows a table on the given provider, surviving the
# asynchronous re-taste after a spoiled consumer.
wait_for_table()
{
	local n=0

	until gpart show $1 >/dev/null 2>&1 || [ ${n} -ge 20 ]; do
		sleep 0.25
		n=$((n + 1))
	done
}

zoned_part_cleanup()
{
	if [ -f "$TEST_MDS_FILE" ]; then
		while read md; do
			gpart destroy -F ${md}.zoned 2>/dev/null
			[ -c /dev/${md}.zoned ] && \
			    gzoned destroy ${md}.zoned 2>/dev/null
			mdconfig -d -u $md 2>/dev/null
		done < $TEST_MDS_FILE
	fi
	true
}

atf_test_case create cleanup
create_head()
{
	atf_set "descr" "GPT on a zoned device writes only conventional zones"
	atf_set "require.user" "root"
	atf_set "require.progs" "gzoned zonectl"
}
create_body()
{
	zoned_part_test_setup

	# Common layout: conventional zones at the head only.
	alloc_zoned_md 0
	atf_check -o ignore gpart create -s gpt ${md}.zoned
	atf_check -o ignore gpart add -t freebsd-ufs ${md}.zoned
	atf_check test -c /dev/${md}.zonedp1
	# Omitted backup table: the three sequential zones, including the one
	# holding the last LBA, are still empty.
	atf_check_equal "3" "$(zone_count /dev/${md}.zoned empty)"
}
create_cleanup()
{
	zoned_part_cleanup
}

atf_test_case retaste cleanup
retaste_head()
{
	atf_set "descr" "A table without its backup does not taste corrupt"
	atf_set "require.user" "root"
	atf_set "require.progs" "gzoned zonectl"
}
retaste_body()
{
	zoned_part_test_setup

	alloc_zoned_md 0
	atf_check -o ignore gpart create -s gpt ${md}.zoned
	# Spoil the consumer, forcing the table to be re-read from disk.
	atf_check sh -c "true > /dev/${md}.zoned"
	wait_for_table ${md}.zoned
	atf_check -o not-match:"CORRUPT" gpart show ${md}.zoned
}
retaste_cleanup()
{
	zoned_part_cleanup
}

atf_test_case scheme_refused cleanup
scheme_refused_head()
{
	atf_set "descr" "Schemes without metadata ranges are refused up-front"
	atf_set "require.user" "root"
	atf_set "require.progs" "gzoned zonectl"
}
scheme_refused_body()
{
	zoned_part_test_setup

	alloc_zoned_md 0
	atf_check -s not-exit:0 -e match:"host-managed" \
	    gpart create -s mbr ${md}.zoned
	# The refusal must happen before anything is written: the
	# sequential zones are still empty.
	atf_check_equal "3" "$(zone_count /dev/${md}.zoned empty)"
}
scheme_refused_cleanup()
{
	zoned_part_cleanup
}

atf_test_case seq_head_refused cleanup
seq_head_refused_head()
{
	atf_set "descr" "GPT is refused when the primary cannot be written"
	atf_set "require.user" "root"
	atf_set "require.progs" "gzoned zonectl"
}
seq_head_refused_body()
{
	zoned_part_test_setup

	# No conventional zones at all: even the primary has no home.
	alloc_zoned_md
	atf_check -s not-exit:0 -e match:"non-conventional" \
	    gpart create -s gpt ${md}.zoned
	atf_check_equal "4" "$(zone_count /dev/${md}.zoned empty)"
}
seq_head_refused_cleanup()
{
	zoned_part_cleanup
}

atf_test_case conv_backup cleanup
conv_backup_head()
{
	atf_set "descr" "A conventional drive tail receives the backup table"
	atf_set "require.user" "root"
	atf_set "require.progs" "gzoned zonectl"
}
conv_backup_body()
{
	local lbas

	zoned_part_test_setup

	alloc_zoned_md 0-3
	atf_check -o ignore gpart create -s gpt ${md}.zoned
	lbas=$(diskinfo /dev/${md}.zoned | awk '{print $4}')
	atf_check -o match:"EFI PART" sh -c \
	    "dd if=/dev/${md}.zoned bs=512 skip=$((lbas - 1)) count=1 \
	    2>/dev/null | strings"
}
conv_backup_cleanup()
{
	zoned_part_cleanup
}

atf_test_case destroy cleanup
destroy_head()
{
	atf_set "descr" "Destroying a table skips the unwritable tail scrub"
	atf_set "require.user" "root"
	atf_set "require.progs" "gzoned zonectl"
}
destroy_body()
{
	zoned_part_test_setup

	alloc_zoned_md 0
	atf_check -o ignore gpart create -s gpt ${md}.zoned
	atf_check -o ignore gpart destroy ${md}.zoned
	atf_check -s not-exit:0 -o ignore -e ignore gpart show ${md}.zoned
	atf_check_equal "3" "$(zone_count /dev/${md}.zoned empty)"
}
destroy_cleanup()
{
	zoned_part_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case create
	atf_add_test_case retaste
	atf_add_test_case scheme_refused
	atf_add_test_case seq_head_refused
	atf_add_test_case conv_backup
	atf_add_test_case destroy
}
