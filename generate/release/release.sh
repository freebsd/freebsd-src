#!/bin/bash

#******************************************************************************
#
# ACPICA release generation script for Cygwin/Windows execution
#
# front end for build.sh
#
# Copies any existing packages to the archive directory.
#
# Generates 3 types of package:
#   1) Standard ACPICA source, everything except test suites
#   2) ACPICA test suites (very large)
#   3) Windows binary tools (Windows does not include generation tools)
#
# Note: "unix" generation builds the source with the standard Intel license
# in each file header. "unix2" builds the source with the dual license instead.
# this has been requested by some OS vendors, notably FreeBSD.
#
#******************************************************************************

# Configuration

NPARAM=$#
BUILD_TESTS=1

# Filenames and paths

ARCHIVE_DIR=archive
RELEASE_DIR=current


#******************************************************************************
#
# Miscellaneous utility functions
#
#******************************************************************************

usage()
{
	echo "$1"
	echo
	echo "Master script to create ACPICA release packages"
	echo "Usage:"
	echo "    $0 [notest]"
}

move_all_files_to_archive()
{
	cd $RELEASE_DIR

	for file in *
	do
		if [ -d $file ]; then
			rm -r -f ../$ARCHIVE_DIR/$file
			mv -f $file ../$ARCHIVE_DIR
			echo "Moved directory $file to $ARCHIVE_DIR directory"
		else
			cp $file ../$ARCHIVE_DIR
			echo "Moved $file ($(ls -al $file | awk '{print $5}') bytes) to $ARCHIVE_DIR directory"
			rm $file
		fi
	done

	cd ..
}


#******************************************************************************
#
# main
#
# Arguments:
#    $1 (optional) notest - do not generate the ACPICA test suite packages
#
#******************************************************************************

set -e		# Abort on any error

#
# Parameter evaluation
#
if [ $NPARAM -gt 1 ]; then
	usage "Wrong argument count ($NPARAM)"
	exit 1
	
elif [ $NPARAM -eq 1 ]; then
	if [ $1 == notest ]; then
		BUILD_TESTS=0
	else
		usage "Invalid argument ($1)"
		exit 1
	fi
fi

#
# Move and preserve any previous versions of the various release packages
#
if [ -e $RELEASE_DIR ]; then

	# Create archive directory if necessary

	mkdir -p $ARCHIVE_DIR

	#
	# Save any older versions of the release packages
	#
	if [ "$(ls -A $RELEASE_DIR)" ]; then
		echo "Moving previous packages to $ARCHIVE_DIR directory"

		move_all_files_to_archive
		echo "Completed move of previous packages to $ARCHIVE_DIR directory"
	fi

else
	# Just create the release directory
	mkdir -p $RELEASE_DIR
fi

# ACPICA source code (core subsystem and all tools/utilities)

bash build.sh source win
bash build.sh source unix
bash build.sh source unix2

# Optionally build the test suite packages (built by default)

if [ $BUILD_TESTS -eq 1 ]; then

	# ACPICA test suites (A unix2 build has not been requested by users)

	bash build.sh test win
	bash build.sh test unix
	
else
	echo "**** Test suites not built because the notest option was used"
fi

# ACPICA binary tools (Windows only)

bash build.sh binary win

echo
echo "ACPICA - Summary of generated packages:"
echo
ls $RELEASE_DIR -g -G -t
