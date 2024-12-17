#/*
# *  Copyright (C) 2017 - This file is part of libecc project
# *
# *  Authors:
# *      Ryad BENADJILA <ryadbenadjila@gmail.com>
# *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
# *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
# *
# *  Contributors:
# *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
# *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
# *
# *  This software is licensed under a dual BSD and GPL v2 license.
# *  See LICENSE file at the root folder of the project.
# */
#!/bin/sh

run_triplet_wordsize(){
	triplet=$1
	wordsize=$2
	echo "======== RUNNING RELEASE FOR $triplet / $wordsize"
        if [ "$triplet" != "i386-apple-darwin" ] && [ "$triplet" != "x86_64-apple-darwin" ] && [ "$triplet" != "x86_64h-apple-darwin" ] && [ "$triplet" != "i686-w64-mingw32" ] && [ "$triplet" != "x86_64-w64-mingw32" ]; then
		echo "  [X] Using QEMU static"
		$CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_static vectors
		$CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_debug_static vectors
		#$CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_static rand
	fi
	if [ "$triplet" = "i386-apple-darwin" ] || [ "$triplet" = "x86_64-apple-darwin" ] || [ "$triplet" = "x86_64h-apple-darwin" ]; then
		echo "  [X] Testing MAC-OS binaries is futur work!"
	fi
	if [ "$triplet" = "i686-w64-mingw32" ] || [ "$triplet" = "x86_64-w64-mingw32" ]; then
		echo "  [X] Using WINE"
		wine $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_static vectors
		wine $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_debug_static vectors
		#wine $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_static rand
	fi
}


print_help(){
	echo "$0 uses qemu-static and wine to run self tests"
	echo "with multiple word sizes (16, 32 and 64)."
	echo "The produced binaries are expected to be in the 'crossbuild_out' folder."
	echo "Supported platform triplets are:"
	echo "arm-linux-gnueabi / arm-linux-gnueabihf / powerpc64le-linux-gnu / aarch64-linux-gnu /"
	echo "mipsel-linux-gnu / i386-apple-darwin / x86_64-apple-darwin / i686-w64-mingw32 / x86_64-w64-mingw32."
}


######### Script main
SRC_DIR=`dirname "$(readlink -f "$0")"`/..
CROSSBUILD_OUTPUT=$SRC_DIR/scripts/crossbuild_out/

# Check for the qemu-static command line
for arch in i386 x86_64 arm ppc64 aarch64 mipsel;
do
	QEMU_CHECK=$(qemu-$arch-static -h)
	if [ $? -ne 0 ]; then
		echo "qemu-$arch-static is not installed ... Please install it! (usually in qemu-user-static)"
		exit
	fi
done

WINE_CHECK=$(wine --help)
if [ $? -ne 0 ]; then
	echo "wine is not installed ... Please install it!"
	exit
fi

WINE64_CHECK=$(wine64 --help)
if [ $? -ne 0 ]; then
	echo "wine64 is not installed ... Please install it!"
	exit
fi

# Print help if asked
if [ "$1" = "-h" ]
then
	print_help $0
	exit
fi

# If we have arguments, just execute subcommand
if [ "$1" = "-triplet" ]
then
	# If no specific word size has been given, do all the sizes
	if [ "$3" = "" ]
	then
		for wordsize in 16 32 64;
		do
			run_triplet_wordsize $2 $wordsize
		done
	else
		run_triplet_wordsize $2 $3
	fi
	exit
fi

ALL_CHECKS=""
for wordsize in 16 32 64;
do
	for triplet in arm-linux-gnueabi arm-linux-gnueabihf powerpc64le-linux-gnu aarch64-linux-gnu mipsel-linux-gnu i386-apple-darwin x86_64-apple-darwin x86_64h-apple-darwin i686-w64-mingw32 x86_64-w64-mingw32;
	do
		ALL_CHECKS="$ALL_CHECKS\n-triplet $triplet $wordsize"
	done
done

if [ "$1" = "-cpu" ]
then
	if [ "$3" = "-triplet" ]
	then
		echo "-cpu and -triplet are not compatible ..."
		exit
	else
		# User defined number of CPUs
		NCPU=$2
	fi
else
	# Get number of CPUs for parallel processing
	NCPU=`getconf _NPROCESSORS_ONLN`
fi
echo "Parallelizing on $NCPU processors"
# Unleash the kraken
echo $ALL_CHECKS | xargs -n 4 -P $NCPU sh `readlink -f "$0"`
