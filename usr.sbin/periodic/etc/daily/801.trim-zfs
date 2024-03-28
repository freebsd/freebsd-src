#!/bin/sh
#
#

# If there is a global system configuration file, suck it in.
#

if [ -r /etc/defaults/periodic.conf ]
then
    . /etc/defaults/periodic.conf
    source_periodic_confs
fi

case "$daily_trim_zfs_enable" in
    [Yy][Ee][Ss])
	echo
	echo 'Trimming of zfs pools:'

	if [ -z "${daily_trim_zfs_pools}" ]; then
		daily_trim_zfs_pools="$(zpool list -H -o name)"
	fi

	rc=0
	for pool in ${daily_trim_zfs_pools}; do
		# sanity check
		_status=$(zpool list -Hohealth "${pool}" 2> /dev/null)
		if [ $? -ne 0 ]; then
			rc=2
			echo "   WARNING: pool '${pool}' specified in"
			echo "            '/etc/periodic.conf:daily_trim_zfs_pools'"
			echo "            does not exist"
			continue
		fi
		case ${_status} in
		FAULTED)
			rc=3
			echo "Skipping faulted pool: ${pool}"
			continue ;;
		UNAVAIL)
			rc=4
			echo "Skipping unavailable pool: ${pool}"
			continue ;;
		esac

		if ! zpool status "${pool}" | grep -q '(trimming)'; then
			echo "    starting trim of pool '${pool}'"
			zpool trim ${daily_zfs_trim_flags} "${pool}"
		else
			echo "    trim of pool '${pool}' already in progress, skipping"
		fi
	done
	;;

    *)
	rc=0
	;;
esac

exit $rc
