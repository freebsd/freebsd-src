#!/bin/sh
#
# This script builds X 3.3.x from the XFree86 and XFree86-contrib ports and
# installs it into a work directory.  Once that is done, it uses XFree86's
# build-bindist command to package up the binary dists leaving them stored
# in the 'dist/bindist' subdirectory of the specified output directory.

# usage information
#
usage() {
	echo "$0 <work dir> <output dir>"
	echo
	echo "Where <output dir> is the base directory to install X into,"
	echo "and <work dir> is a scratch directory that the XFree86 ports"
	echo "can be checked out into and built.  This script also assumes"
	echo "that it can get the distfiles from /usr/ports/distfiles (or"
	echo "fetch them into that directory).  The CVSROOT environment"
	echo "variable should point to a FreeBSD CVS repository."
	echo
	echo "Before running this script, the following packages should be"
	echo "installed:"
	echo "	XFree86"
	echo "	Tcl83, Tk83"
	echo "	ja-Tcl80, ja-Tk80"
	echo
	echo "Also, this should really be run as root."
	exit 1
}

# check the command line
if [ $# -ne 2 ]; then
	usage
fi

# check $CVSROOT
if [ -z "$CVSROOT" ]; then
	echo "\$CVSROOT not set!"
	echo
	usage
fi

# setup the output directory
output_dir=$2
echo ">>> preparing output directory: ${output_dir}"
case $output_dir in
	/*)
		;;
	*)
		output_dir=`pwd`/${output_dir}
		;;
esac
if [ -r ${output_dir} ]; then
	if ! rm -rf ${output_dir}; then
		echo "Could not remove ${output_dir}!"
		echo
		usage
	fi
fi
if ! mkdir -p ${output_dir}; then
	echo "Could not create ${output_dir}!"
	echo
	usage
fi
if ! rmdir ${output_dir}; then
	echo "Could not remove ${output_dir} the second time!"
	echo
	usage
fi

# setup the work directory
work_dir=$1
echo ">>> preparing work directory: ${work_dir}"
if [ -r ${work_dir} ]; then
	if ! rm -rf ${work_dir}; then
		echo "Could not remove ${work_dir}!"
		echo
		usage
	fi
fi
if ! mkdir -p ${work_dir}; then
	echo "Could not create ${work_dir}!"
	echo
	usage
fi
if ! mkdir ${work_dir}/base; then
	echo "Could not create ${work_dir}/base!"
	echo
	usage
fi
if ! mkdir ${work_dir}/dist; then
	echo "Could not create ${work_dir}/dist!"
	echo
	usage
fi
if ! mkdir ${work_dir}/ports; then
	echo "Could not create ${work_dir}/ports!"
	echo
	usage
fi

# check out the XFree86 and XFree86-contrib ports and set them up
echo ">>> checking out ports"
if ! ( cd ${work_dir}/ports && \
    cvs -R -d ${CVSROOT} co -P XFree86 XFree86-contrib ); then
	echo "Could not checkout the XFree86 port!"
	echo
	usage
fi
if [ -r  XF86.patch ]; then
	echo ">>> patching ports"
	if ! patch -d ${work_dir}/ports/XFree86 < XF86.patch; then
		echo "Could not patch the XFree86 port!"
		echo
		usage
	fi
fi

# actually build X
echo ">>> building X"
if ! ( cd ${work_dir}/ports/XFree86 && \
    make BUILD_XDIST=yes DISTDIR=/usr/ports/distfiles \
    DESTDIR=${work_dir}/base NO_PKG_REGISTER=yes all install ); then
	echo "Could not build XFree86!"
	echo
	usage
fi
if ! ( cd ${work_dir}/ports/XFree86-contrib && \
    make DISTDIR=/usr/ports/distfiles DESTDIR=${work_dir}/base \
    NO_PKG_REGISTER=yes all install ); then
	echo "Could not build XFree86-contrib!"
	echo
	usage
fi

# now package up the bindists
echo ">>> building bindist"
bindist_dir=${work_dir}/ports/XFree86/work/xc/programs/Xserver/hw/xfree86/etc/bindist
if ! cp ${bindist_dir}/FreeBSD-ELF/* ${work_dir}/dist; then
	echo "Could not copy over distribution lists!"
	echo
	usage
fi
if ! cp ${bindist_dir}/common/* ${work_dir}/dist; then
	echo "Could not copy over distribution lists!"
	echo
	usage
fi
if ! ${bindist_dir}/build-bindist X ${work_dir}/base ${work_dir}/dist; then
	echo "Could not package up binary dists!"
	echo
	usage
fi
if ! mv ${work_dir}/dist/bindist ${output_dir}; then
	echo "Could not move binary dists into ${output_dir}!"
	echo
	usage
fi
