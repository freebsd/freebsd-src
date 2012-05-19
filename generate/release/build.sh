#!/bin/bash

#******************************************************************************
#
# ACPICA package generation script for Cygwin/Windows execution
#
# Requires cygwin be installed - http://www.cygwin.com
# and its /bin be *first* in your path.
#
# Windows packages require pkzip25 (free, and is available from numerous
# sources - search for "pkzip25" or "pkzip25.exe")
#
# Execute this script from the acpica/generate/release directory.
#
# Constructed packages are placed in the acpica/generate/release/current
# directory.
#
# Line Terminators: Windows source packages leave the CR/LF terminator.
# Unix packages convert the CR/LF terminators to LF only.
#
# Usage:
#
#   build <package_type> <target_type>
#
#   where:
#       <package_type> is one of:
#           source  - Build an ACPICA source package (core and all tools)
#           test    - Build an ACPICA test suite package
#           binary  - Build an ACPICA binary tools package
#
#       <target_type> is one of:
#           win     - Generate Windows package (Intel license, CRLF line terminators)
#           unix    - Generate Unix package (Intel license, LF line terminators)
#           unix2   - Generate Unix package (dual license, LF line terminators)
#
#******************************************************************************

# Configuration

ZIP_UTILITY="/cygdrive/c/windows/pkzip25.exe"
ACPISRC="libraries/acpisrc.exe"
DOS2UNIX="dos2unix"
UNIX2DOS="unix2dos"

# Filenames and paths

TARGET_DIR="generate/release/current"
TEMP_DIR=acpitemp
TEST_PREFIX=acpitests
SOURCE_PREFIX=acpica
BINARY_PREFIX=iasl
PACKAGE_SUFFIX=`date +%Y%m%d`

NPARAM=$#


#******************************************************************************
#
# Miscellaneous utility functions
#
#******************************************************************************

usage()
{
	echo "$1"
	echo
	echo "Low-level build script for ACPICA release packages"
	echo "Usage:"
	echo "    $0 source <win | unix | unix2>"
	echo "    $0 test   <win | unix>"
	echo "    $0 binary <win>"
}

banner()
{
	echo
	echo "$1"
	echo
}

check_zip_utility_exists()
{
	#
	# Need pkzip (or similar) to build the windows packages
	#
	if [ ! -e "$ZIP_UTILITY" ]; then
		echo "ZIP_UTILITY ($ZIP_UTILITY) does not exist!"
		exit 1
	fi
}

convert_to_unix_line_terminators()
{
	#
	# Convert all CR/LF pairs to Unix format (LF only)
	#
	cd $TEMP_DIR
	echo Starting CR/LF to LF Conversion
	find . -name "*" | xargs $DOS2UNIX
	echo Completed CR/LF to LF Conversion
	cd ..
}

convert_to_dos_line_terminators()
{
	#
	# Convert all lone LF terminators to CR/LF
	# Note: Checks shell scripts only (*.sh)
	#
	cd $TEMP_DIR
	echo Starting LF to CR/LF Conversion
	find . -name "*.sh" | xargs $UNIX2DOS
	echo Completed LF to CR/LF Conversion
	cd ..
}

insert_dual_license_headers()
{
	#
	# Need acpisrc utility to insert the headers
	#
	if [ ! -e "$ACPISRC" ]; then
		echo "acpisrc ($ACPISRC) does not exist!"
		exit 1
	fi

	#
	# Insert the dual license into *.c and *.h files
	#
	echo "Inserting dual-license into all source files"
	$ACPISRC -h -y $TEMP_DIR
}

build_unix_package()
{
	convert_to_unix_line_terminators

	#
	# Build release package
	#
	rm -r -f $PACKAGE_FILENAME
	mv $TEMP_DIR $PACKAGE_FILENAME
	tar czf $PACKAGE_FILENAME.tar.gz $PACKAGE_FILENAME

	#
	# Move the completed package
	#
	mv $PACKAGE_FILENAME.tar.gz $TARGET_DIR
	mv $PACKAGE_FILENAME $TEMP_DIR
}

build_windows_package()
{
	convert_to_dos_line_terminators

	#
	# Build release package
	#
	cd $TEMP_DIR
	rm -r -f ../$TARGET_DIR/$PACKAGE_FILENAME
	$ZIP_UTILITY -add -max -dir -sort=name ../$TARGET_DIR/$PACKAGE_FILENAME
	cd ..
}


#******************************************************************************
#
# generate_source_package
#
# Generates the ACPICA source code packages (core and all tools)
#
# Arguments:
#   %1  - Target type (win or unix or unix2)
#
#******************************************************************************

generate_source_package ()
{
	#
	# Parameter evaluation
	#
	if [ $1 == win ]; then
		PACKAGE_NAME=Windows
		PACKAGE_TYPE=Win
		LICENSE=Intel
		check_zip_utility_exists

	elif [ $1 == unix ]; then
		PACKAGE_NAME="Unix (Intel License)"
		PACKAGE_TYPE=Unix
		LICENSE=Intel

	elif [ $1 == unix2 ]; then
		PACKAGE_NAME="Unix (Dual License)"
		PACKAGE_TYPE=Unix
		LICENSE=Dual

	else
		usage "Invalid argument ($1)"
		exit 1
	fi

	PACKAGE_FILENAME=$SOURCE_PREFIX-$1-$PACKAGE_SUFFIX
	banner "ACPICA - Generating $PACKAGE_NAME source code package ($PACKAGE_FILENAME)"

	#
	# Make directories common to all source packages
	#
	mkdir $TEMP_DIR
	mkdir $TEMP_DIR/libraries
	mkdir $TEMP_DIR/generate
	mkdir $TEMP_DIR/generate/lint
	mkdir $TEMP_DIR/generate/release
	mkdir $TEMP_DIR/generate/unix
	mkdir $TEMP_DIR/generate/unix/acpibin
	mkdir $TEMP_DIR/generate/unix/acpiexec
	mkdir $TEMP_DIR/generate/unix/acpihelp
	mkdir $TEMP_DIR/generate/unix/acpinames
	mkdir $TEMP_DIR/generate/unix/acpisrc
	mkdir $TEMP_DIR/generate/unix/acpixtract
	mkdir $TEMP_DIR/generate/unix/iasl
	mkdir $TEMP_DIR/tests
	mkdir $TEMP_DIR/tests/misc
	mkdir $TEMP_DIR/tests/templates
	mkdir -p $TEMP_DIR/source/os_specific/service_layers

	#
	# Copy ACPICA subsystem source code
	#
	cp -r documents/changes.txt             $TEMP_DIR/changes.txt
	cp -r source/common                     $TEMP_DIR/source/common
	cp -r source/components                 $TEMP_DIR/source/
	cp -r source/include                    $TEMP_DIR/source/include
	cp -r generate/release/*.sh             $TEMP_DIR/generate/release

	#
	# Copy iASL compiler and tools source
	#
	cp -r source/compiler                   $TEMP_DIR/source/compiler
	cp -r source/tools                      $TEMP_DIR/source/tools

	#
	# Copy iASL/ACPICA miscellaneous tests (not full test suites)
	#
	cp -r tests/misc/*.asl                  $TEMP_DIR/tests/misc
	cp -r tests/templates/Makefile          $TEMP_DIR/tests/templates
	cp -r tests/templates/templates.sh      $TEMP_DIR/tests/templates

	#
	# Copy all OS-specific interfaces
	#
	cp source/os_specific/service_layers/*.c $TEMP_DIR/source/os_specific/service_layers

	#
	# Copy generic UNIX makefiles
	#
	cp generate/unix/readme.txt             $TEMP_DIR/generate/unix/readme.txt
	cp generate/unix/Makefile*              $TEMP_DIR/generate/unix
	cp generate/unix/acpibin/Makefile       $TEMP_DIR/generate/unix/acpibin
	cp generate/unix/acpiexec/Makefile      $TEMP_DIR/generate/unix/acpiexec
	cp generate/unix/acpihelp/Makefile      $TEMP_DIR/generate/unix/acpihelp
	cp generate/unix/acpinames/Makefile     $TEMP_DIR/generate/unix/acpinames
	cp generate/unix/acpisrc/Makefile       $TEMP_DIR/generate/unix/acpisrc
	cp generate/unix/acpixtract/Makefile    $TEMP_DIR/generate/unix/acpixtract
	cp generate/unix/iasl/Makefile          $TEMP_DIR/generate/unix/iasl

	#
	# Copy Lint directory
	#
	cp -r generate/lint $TEMP_DIR/generate
	rm -f $TEMP_DIR/generate/lint/co*
	rm -f $TEMP_DIR/generate/lint/env*
	rm -f $TEMP_DIR/generate/lint/lib*
	rm -f $TEMP_DIR/generate/lint/LintOut.txt

	if [ $PACKAGE_TYPE == Unix ]; then
		#
		# Unix/Linux-specific activities
		#
		
		# Copy Linux/UNIX utility generation makefiles

		cp generate/linux/Makefile.acpibin      $TEMP_DIR/source/tools/acpibin/Makefile
		cp generate/linux/Makefile.acpiexec     $TEMP_DIR/source/tools/acpiexec/Makefile
		cp generate/linux/Makefile.acpihelp     $TEMP_DIR/source/tools/acpihelp/Makefile
		cp generate/linux/Makefile.acpinames    $TEMP_DIR/source/tools/acpinames/Makefile
		cp generate/linux/Makefile.acpisrc      $TEMP_DIR/source/tools/acpisrc/Makefile
		cp generate/linux/Makefile.acpixtract   $TEMP_DIR/source/tools/acpixtract/Makefile
		cp generate/linux/Makefile.iasl         $TEMP_DIR/source/compiler/Makefile
		cp generate/linux/README.acpica-unix    $TEMP_DIR/README

		#
		# For Unix2 case, insert the dual license header into all source files
		#
		if [ $LICENSE == Dual ]; then
			insert_dual_license_headers
		fi

		build_unix_package

	else
		#
		# Windows-specific activities
		#

		# Copy project files for MS Visual Studio 2008 (VC++ 9.0)

		mkdir $TEMP_DIR/generate/msvc9
		cp -r generate/msvc9/*.sln $TEMP_DIR/generate/msvc9/
		cp -r generate/msvc9/*.vcproj $TEMP_DIR/generate/msvc9/

		build_windows_package
	fi

	banner "ACPICA - Completed $PACKAGE_NAME source code package ($PACKAGE_FILENAME)"
}


#******************************************************************************
#
# generate_test_package
#
# Generates the ACPICA test suite packages
#
# Arguments:
#   %1  - Target type (win or unix)
#
#******************************************************************************

generate_test_package()
{
	#
	# Parameter evaluation
	#
	if [ $1 == win ]; then
		PACKAGE_NAME=Windows
		PACKAGE_TYPE=Win
		check_zip_utility_exists

	elif [ $1 == unix ]; then
		PACKAGE_NAME="Unix"
		PACKAGE_TYPE=Unix

	else
		usage "Invalid argument ($1)"
		exit 1
	fi

	PACKAGE_FILENAME=$TEST_PREFIX-$1-$PACKAGE_SUFFIX
	banner "ACPICA - Generating $PACKAGE_NAME test suite package ($PACKAGE_FILENAME)"

	#
	# Copy the ASL Test source
	#
	mkdir $TEMP_DIR
	cp -r tests $TEMP_DIR/tests

	#
	# Delete extraneous files
	#
	cd $TEMP_DIR
	find . -name "tmp" | xargs rm -r -f
	find . -name "aml" | xargs rm -r -f
	find . -name "CVS" | xargs rm -r -f
	cd ..

	if [ $PACKAGE_TYPE == Unix ]; then
		#
		# Unix/Linux-specific activities
		#
		build_unix_package

	else
		#
		# Windows-specific activities
		#
		build_windows_package
	fi

	banner "ACPICA - Completed $PACKAGE_NAME test suite package ($PACKAGE_FILENAME)"
}


#******************************************************************************
#
# generate_binary_package
#
# Generates the ACPICA binary package (Currently Windows only)
#
# Arguments:
#   %1  - Target type (win)
#
#******************************************************************************

generate_binary_package()
{
	#
	# Parameter evaluation
	#
	if [ $1 == win ]; then
		PACKAGE_NAME=Windows
		PACKAGE_TYPE=Win
		check_zip_utility_exists

	else
		usage "Invalid argument ($1)"
		exit 1
	fi

	PACKAGE_FILENAME=$BINARY_PREFIX-$1-$PACKAGE_SUFFIX
	banner "ACPICA - Generating $PACKAGE_NAME binary tools package ($PACKAGE_FILENAME)"

	#
	# Copy executables and documentation
	#
	mkdir $TEMP_DIR
	cp -r documents/changes.txt     $TEMP_DIR/changes.txt
	cp documents/aslcompiler.pdf    $TEMP_DIR
	cp libraries/acpibin.exe        $TEMP_DIR
	cp libraries/acpiexec.exe       $TEMP_DIR
	cp libraries/acpihelp.exe       $TEMP_DIR
	cp libraries/acpinames.exe      $TEMP_DIR
	cp libraries/acpisrc.exe        $TEMP_DIR
	cp libraries/acpixtract.exe     $TEMP_DIR
	cp libraries/iasl.exe           $TEMP_DIR
	cp tests/misc/badcode.asl       $TEMP_DIR

	build_windows_package
	banner "ACPICA - Completed $PACKAGE_NAME binary tools package ($PACKAGE_FILENAME)"
}


#******************************************************************************
#
# main
#
# Arguments:
#       $1 (package_type) is one of:
#           source  - Build an ACPICA source package (core and all tools)
#           test    - Build an ACPICA test suite package
#           binary  - Build an ACPICA binary tools package
#
#       $2 (target_type) is one of:
#           win     - Generate Windows package (Intel license, CRLF line terminators)
#           unix    - Generate Unix package (Intel license, LF line terminators)
#           unix2   - Generate Unix package (dual license, LF line terminators)
#
#******************************************************************************

set -e		# Abort on any error

if [ $NPARAM -ne 2 ]; then
	usage "Wrong argument count ($NPARAM)"
	exit 1
fi

#
# cd from acpica/generate/release to acpica
#
cd ../..

#
# Ensure that the temporary directory is created fresh
#
rm -rf $TEMP_DIR
		
#
# Parameter evaluation
#
if [ $1 == source ]; then
	generate_source_package $2

elif [ $1 == test ]; then
	generate_test_package $2

elif [ $1 == binary ]; then
	generate_binary_package $2

else
	usage "Invalid argument ($1)"
	exit 1
fi

#
# Remove temporary directory
#
rm -rf $TEMP_DIR
