#!/bin/sh
#
# Builds X from the port and stores it under the specified directory.

# usage information
#
usage() {
	echo "$0 <output dir>"
	echo
	echo "Where <output dir> is the base directory to install X into.  This"
	echo "script also assumes that it can checkout XFree86 into "
	echo `dirname $0`"/XFree86 and that it can get the distfiles from"
	echo "/usr/ports/distfiles (or fetch them into that directory)."
	echo
	echo "Also, this should really be run as root."
	exit 1
}

# check the command line
if [ $# -ne 1 ]; then
	usage
fi

# setup the output dir
output_dir=$1
case $output_dir in
	/*)
		;;
	*)
		output_dir=`pwd`/${output_dir}
		;;
esac
if ! mkdir -p $1; then
	echo "Could not create ${output_dir}!"
	echo
	usage
fi

# extract the directory this script lives in
home_dir=`dirname $0`

# check out the XFree86 and XFree86-contrib ports and set them up
if ! ( cd $home_dir && cvs -R -d ${CVSROOT} co -P XFree86 XFree86-contrib && \
		cd XFree86 && patch < ../XF86.patch ); then
	echo "Could not checkout the XFree86 port!"
	echo
	usage
fi

# actually build X
if ! ( cd $home_dir/XFree86 && \
		make DISTDIR=/usr/ports/distfiles DESTDIR=${output_dir} \
		NO_PKG_REGISTER=yes all install ); then
	echo "Could not build XFree86!"
	echo
	usage
fi
if ! ( cd $home_dir/XFree86-contrib && \
		make DISTDIR=/usr/ports/distfiles DESTDIR=${output_dir} \
		NO_PKG_REGISTER=yes all install ); then
	echo "Could not build XFree86-contrib!"
	echo
	usage
fi
