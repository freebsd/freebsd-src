#!/bin/sh
# Created by: Miroslav Lachman <000.fbsd@quip.cz>

# Backup of zpool list, zfs list, zpool properties and zfs properties
# for each filesystem. The backup will be stored in /var/backups.

# If there is a global system configuration file, suck it in.
#
if [ -r /etc/defaults/periodic.conf ]
then
	. /etc/defaults/periodic.conf
	source_periodic_confs
fi

bak_dir=/var/backups
rc=0

rotate() {
	base_name=$1
	show_diff=$2
	file="$bak_dir/$base_name"

	if [ -f "${file}.bak" ] ; then
		if cmp -s "${file}.bak" "${file}.tmp"; then
			rm "${file}.tmp"
		else
			if [ -n "$show_diff" ]; then
				rc=1
				diff ${daily_diff_flags} "${file}.bak" "${file}.tmp"
			fi
			mv "${file}.bak" "${file}.bak2" || rc=3
			mv "${file}.tmp" "${file}.bak" || rc=3
		fi
	else
		rc=1
		mv "${file}.tmp" "${file}.bak" || rc=3
		[ -n "$show_diff" ] && cat "${file}.bak"
	fi
}

show=""
case "$daily_backup_zfs_verbose" in
	[Yy][Ee][Ss]) show="YES"
esac

case "$daily_backup_zfs_enable" in
	[Yy][Ee][Ss])

	zpools=$(zpool list $daily_backup_zpool_list_flags)

	if [ -z "$zpools" ]; then
		echo 'daily_backup_zfs_enable is set to YES but no zpools found.'
		rc=2
	else
		echo ""
		echo "Backup of ZFS information for all imported pools";

		echo "$zpools" > "$bak_dir/zpool_list.tmp"
		rotate "zpool_list" $show

		zfs list $daily_backup_zfs_list_flags > "$bak_dir/zfs_list.tmp"
		rotate "zfs_list" $show
	fi
	;;
esac

case "$daily_backup_zfs_props_enable" in
	[Yy][Ee][Ss])

	zfs get $daily_backup_zfs_get_flags > "$bak_dir/zfs_props.tmp"
	rotate "zfs_props" $show

	zpool get $daily_backup_zpool_get_flags > "$bak_dir/zpool_props.tmp"
	rotate "zpool_props" $show
	;;
esac

exit $rc
