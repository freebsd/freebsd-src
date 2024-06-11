#!/bin/sh
# Created by: Miroslav Lachman <000.fbsd@quip.cz>

# Backup output from `gmirror list`, which provides detailed information
# of all gmirrors. The backup will be stored in /var/backups/.

# If there is a global system configuration file, suck it in.
#
if [ -r /etc/defaults/periodic.conf ]
then
	. /etc/defaults/periodic.conf
	source_periodic_confs
fi

bak_dir=/var/backups

rotate() {
	base_name=$1
	show_diff=$2
	file="$bak_dir/$base_name"

	if [ -f "${file}.bak" ]; then
		rc=0
		if cmp -s "${file}.bak" "${file}.tmp"; then
			rm "${file}.tmp"
		else
			rc=1
			[ -n "$show_diff" ] && diff ${daily_diff_flags} "${file}.bak" "${file}.tmp"
			mv "${file}.bak" "${file}.bak2" || rc=3
			mv "${file}.tmp" "${file}.bak" || rc=3
		fi
	else
		rc=1
		mv "${file}.tmp" "${file}.bak" || rc=3
		[ -n "$show_diff" ] && cat "${file}.bak"
	fi
}

case "$daily_backup_gmirror_verbose" in
	[Yy][Ee][Ss]) show="YES"
esac

case "$daily_backup_gmirror_enable" in
	[Yy][Ee][Ss])

	gmirrors=$(gmirror status 2> /dev/null | \
		awk '$1 ~ /^mirror\// { sub(/mirror\//, ""); print $1 }')

	if [ -z "$gmirrors"  ]; then
        echo ""
		echo "daily_backup_gmirror_enable is set to YES but no gmirrors found."
		rc=2
	else
		echo ""
		echo "Backup of gmirror information for:";

		for m in ${gmirrors}; do
			echo "$m"
			safe_name=$(echo "gmirror.${m}" | tr -cs ".[:alnum:]\n" "_")
			if ! gmirror status -s "${m}" | grep -F -v "COMPLETE"; then
				gmirror list "${m}" > "$bak_dir/$safe_name.tmp"
				rotate "$safe_name" $show
			fi
		done
	fi
	;;
	*)  rc=0;;
esac

exit $rc
