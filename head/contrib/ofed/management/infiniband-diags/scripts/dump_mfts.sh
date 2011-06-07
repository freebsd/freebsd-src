#!/bin/sh
#
# This simple script will collect outputs of ibroute for all switches
# on the subnet and drop it on stdout. It can be used for MFTs dump
# generation.
#

usage ()
{
	echo Usage: `basename $0` "[-h] [-D] [-C ca_name]" \
	    "[-P ca_port] [-t(imeout) timeout_ms]"
	exit 2
}

dump_by_lid ()
{
for sw_lid in `ibswitches $ca_info \
		| sed -ne 's/^.* lid \([0-9a-f]*\) .*$/\1/p'` ; do
	ibroute $ca_info -M $sw_lid
done
}

dump_by_dr_path ()
{
for sw_dr in `ibnetdiscover $ca_info -v \
		| sed -ne '/^DR path .* switch /s/^DR path \[\(.*\)\].*$/\1/p' \
		| sed -e 's/\]\[/,/g' \
		| sort -u` ; do
	ibroute $ca_info -M -D ${sw_dr}
done
}

use_d=""
ca_info=""

while [ "$1" ]; do
	case $1 in
	-D)
		use_d="-D"
		;;
	-h)
		usage
		;;
	-P | -C | -t | -timeout)
		case $2 in
		-*)
			usage
			;;
		esac
		if [ x$2 = x ] ; then
			usage
		fi
		ca_info="$ca_info $1 $2"
		shift
		;;
	-*)
		usage
		;;
	*)
		usage
		;;
	esac
	shift
done

if [ "$use_d" = "-D" ] ; then
	dump_by_dr_path
else
	dump_by_lid
fi

exit
