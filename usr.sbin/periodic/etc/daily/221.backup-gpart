#!/bin/sh
## Created by: Miroslav Lachman <000.fbsd@quip.cz>

## Backup of disk partitions layout, useful for gpart restore.
## Data are stored on local filesystem, in /var/backup.
## It is recommended to copy those files to off-site storage.


## If there is a global system configuration file, suck it in.
##
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

	if [ -f "${file}.bak" ] ; then
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

case "$daily_backup_gpart_verbose" in
	[Yy][Ee][Ss]) show="YES"
esac

case "$daily_backup_gpart_enable" in
	[Yy][Ee][Ss])

	echo ""
	echo "Dump of kern.geom.conftxt:";
	sysctl -n kern.geom.conftxt > "$bak_dir/kern.geom.conftxt.tmp"
	rotate "kern.geom.conftxt" $show

	gpart_devs=$(gpart show | awk '$1 == "=>" { print $4 }')
	if [ -n "$daily_backup_gpart_exclude" ]; then
		gpart_devs=$(echo "${gpart_devs}" | grep -E -v "${daily_backup_gpart_exclude}")
	fi

	if [ -z "$gpart_devs"  ]; then
		echo '$daily_backup_gpart_enable is set but no disk probed by kernel.' \
		"perhaps NFS diskless client."
		rc=2
	else
		echo ""
		echo "Backup of partitions information for:";

		for d in ${gpart_devs}; do
			echo "$d"
			safe_name=$(echo "gpart.${d}" | tr -cs ".[:alnum:]\n" "_")
			gpart backup "$d" > "$bak_dir/$safe_name.tmp"
			rotate "$safe_name" $show
		done

		gpart_show=$(gpart show -p)
		boot_part=$(echo "$gpart_show" | awk '$4 ~ /(bios|freebsd)-boot/ { print $3 }')
		if [ -n "$boot_part" ]; then
			echo ""
			echo "Backup of boot partition content:"
			for b in ${boot_part}; do
				echo "$b"
				safe_name=$(echo "boot.${b}" | tr -cs ".[:alnum:]\n" "_")
				dd if="/dev/${b}" of="$bak_dir/$safe_name.tmp" 2> /dev/null
				rotate "$safe_name"
			done
		fi

		mbr_part=$(echo "$gpart_show" | awk '$1 == "=>" && $5 == "MBR" { print $4 }')
		if [ -n "$mbr_part" ]; then
			echo ""
			echo "Backup of MBR record:"
			for mb in ${mbr_part}; do
				echo "$mb"
				safe_name=$(echo "boot.${mb}" | tr -cs ".[:alnum:]\n" "_")
				dd if="/dev/${mb}" of="$bak_dir/$safe_name.tmp" bs=512 count=1 2> /dev/null
				rotate "$safe_name"
			done
		fi

	fi
	;;

	*)  rc=0
	;;
esac

case "$daily_backup_efi_enable" in
    [Yy][Ee][Ss])

    efi_part=$(gpart show -p | awk '$4 ~ /efi/ {print $3}')
    if [ -n "$efi_part" ]; then
        echo ""
        echo "Backup of EFI partition content:"
        for efi in ${efi_part}; do
            echo "$efi"
            safe_name=$(echo "efi.${efi}" | tr -cs ".[:alnum:]\n" "_")
            dd if="/dev/${efi}" of="$bak_dir/$safe_name.tmp" 2> /dev/null
            rotate "$safe_name"
        done
    fi
    ;;
esac

exit $rc
