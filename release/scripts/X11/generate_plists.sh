#!/bin/sh
#
# This script generates the list of files stored in a set of tarballs.  For
# each argument, it uses tar to extract the list of contents and then outputs
# the list to a file with the same base name and the extension "plist".

# generate_plist <tar archive> <packing list>
#
# Takes the archive listed in the first argument and generates a corresponding
# plist file to the name listed in the second argument.
generate_plist() {
	echo "Generating $2 from $1..."

	tar_arguments='tf';

	# handle gzip/bzip2/compress
	case $1 in
		*gz)
			tar_arguments="${tar_arguments}z"
			;;
		*bz)
			tar_arguments="${tar_arguments}y"
			;;
		*Z)
			tar_arguments="${tar_arguments}Z"
			;;
	esac

	tar ${tar_arguments} $1 > $2
}

# output the usage
#
usage() {
	echo "$0 <tarball_dir> <plist_dir>"
	echo
	echo "Where <tarball_dir> is a directory containing all the X tarballs"
	echo "in their proper directory structure and <plist_dir> is a"
	echo "directory to put all the packing lists under."
	exit 1
}

# copy the directory structure of the tarball directory over into the
# packing list directory
#
mirror_directories() {
	echo "Creating packing list directory structure..."
	find ${tarball_dir} -type d | \
		sed -e "s:^${tarball_dir}:mkdir -p ${plist_dir}:" | \
		sh -x || exit 1
}

# build all the package lists
#
build_plists() {
	for archive in `find ${tarball_dir} ! -type d`; do
		plist=`echo ${archive} | \
			sed -e "s/^${tarball_dir}/${plist_dir}/"`.plist
		generate_plist ${archive} ${plist}
	done
}

# check for enough arguments
if [ $# -ne 2 ]; then
	usage
fi

# setup the variables
tarball_dir=$1
plist_dir=$2

# do all the work
if mirror_directories; then
	build_plists
fi
