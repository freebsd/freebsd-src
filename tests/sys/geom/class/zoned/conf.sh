#!/bin/sh

class="zoned"

gzoned_test_setup()
{
	geom_atf_test_setup
}

# Create a vnode-backed md large enough for four 256m zones. gzoned reserves
# space at the end of the provider for its metadata block and zone table, so an
# exact multiple of the zone size (1024m) would lose a zone.
alloc_zoned_md()
{
	atf_check truncate -s 1025m backing_file
	attach_md md -t vnode -f backing_file
}

# Print the write pointer LBA of the zone containing the given LBA.
zone_wp()
{
	zonectl -d /dev/${md}.zoned -c rz -l $1 -P script | \
	    awk -F',' 'NR == 1 {gsub(/ /, "", $3); print $3}'
}

# Print the start LBA of the given zone number.
zone_start()
{
	zonectl -d /dev/${md}.zoned -c rz -P script | \
	    awk -F',' -v n=$(($1 + 1)) 'NR == n {gsub(/ /, "", $1); print $1}'
}

# Print the number of zones matching a report option.
zone_count()
{
	zonectl -d /dev/${md}.zoned -c rz -o $1 -P summary | \
	    awk '/zones,/ {print $1}'
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

