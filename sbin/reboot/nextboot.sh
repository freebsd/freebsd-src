#! /bin/sh
#
# Copyright 2002. Gordon Tetlow.
# gordon@FreeBSD.org
#
# $FreeBSD$

delete="NO"
force="NO"
nextboot_file="/boot/nextboot.conf"
kernel=""

display_usage() {
	echo "Usage: nextboot [-f] [-o options] -k kernel"
	echo "       nextboot -D"
}

# Parse args, do not use getopt because we don't want to rely on /usr
while test $# -gt 0; do
	case $1 in
	-D)
		delete="YES"
		;;
	-f)
		force="YES"
		;;
	-k)
		if test $# -lt 2; then
			echo "$0: option $1 must specify kernel"
			display_usage
			exit 1
		fi
		kernel="$2"
		shift
		;;
	-o)
		if test $# -lt 2; then
			echo "$0: option $1 must specify boot options"
			display_usage
			exit 1
		fi
		kernel_options="$2"
		shift
		;;
	*)
		display_usage
		exit 1
		;;
	esac
	shift
done

if [ ${delete} = "YES" ]; then
	rm -f ${nextboot_file}
	exit 0
fi

if [ "xxx${kernel}" = "xxx" ]; then
	display_usage
	exit 1
fi

if [ ${force} = "NO" -a ! -d /boot/${kernel} ]; then
	echo "Error: /boot/${kernel} doesn't exist. Use -f to override."
	exit 1
fi

cat > ${nextboot_file} << EOF
nextboot_enable="YES"
kernel="${kernel}"
kernel_options="${kernel_options}"
EOF
