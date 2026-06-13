#!/bin/sh

class="zoned"

gzoned_test_setup()
{
	geom_atf_test_setup
}

# Print the write pointer LBA of the zone containing the given LBA.
zone_wp()
{
	zoned_zone_wp /dev/${md}.zoned $1
}

# Print the start LBA of the given zone number.
zone_start()
{
	zoned_zone_start /dev/${md}.zoned $1
}

# Print the number of zones matching a report option.
zone_count()
{
	zoned_zone_count /dev/${md}.zoned $1
}

wait_dev()
{
	for _i in $(seq 1 50); do
		[ -c "$1" ] && return 0
		sleep 0.2
	done
	atf_fail "$1 did not appear"
}

wait_dev_gone()
{
	for _i in $(seq 1 50); do
		[ -c "$1" ] || return 0
		sleep 0.2
	done
	atf_fail "$1 did not go away"
}

gzoned_test_cleanup()
{
	if [ -f "$TEST_MDS_FILE" ]; then
		while read md; do
			[ -c /dev/${md}.zoned ] && \
				gzoned destroy $md.zoned 2>/dev/null
			mdconfig -d -u $md 2>/dev/null
		done < $TEST_MDS_FILE
	fi
	true
}

. `dirname $0`/../geom_subr.sh
. `dirname $0`/../zoned_subr.sh
