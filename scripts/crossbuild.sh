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

# Check if a file $1 exists. Copy it in $2 if
# it exists, or else log and error in $3.
check_and_copy(){
	if [ -e $1 ]
	then
		cp $1 $2
	else
		echo "$2 did not compile ..." >> $3
	fi
}

copy_compiled_examples(){
	ROOT_DIR=$1
	CROSSBUILD_OUTPUT=$2
	triplet=$3
	wordsize=$4
	ERROR_LOG_FILE=$5
	suffix=$6
	# Basic
	check_and_copy $ROOT_DIR/src/examples/basic/nn_pollard_rho $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/nn_pollard_rho_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	check_and_copy $ROOT_DIR/src/examples/basic/fp_square_residue $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/fp_square_residue_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	check_and_copy $ROOT_DIR/src/examples/basic/curve_basic_examples $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/curve_basic_examples_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	check_and_copy $ROOT_DIR/src/examples/basic/curve_ecdh $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/curve_ecdh_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	# Hash
	check_and_copy $ROOT_DIR/src/examples/"hash"/"hash" $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/hash_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	# SSS
	check_and_copy $ROOT_DIR/src/examples/sss/sss $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/sss_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	# Signatures
	check_and_copy $ROOT_DIR/src/examples/sig/rsa/rsa $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/rsa_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	check_and_copy $ROOT_DIR/src/examples/sig/dsa/dsa $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/dsa_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	check_and_copy $ROOT_DIR/src/examples/sig/sdsa/sdsa $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/sdsa_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	check_and_copy $ROOT_DIR/src/examples/sig/kcdsa/kcdsa $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/kcdsa_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
	check_and_copy $ROOT_DIR/src/examples/sig/gostr34_10_94/gostr34_10_94 $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/gostr34_10_94_"$triplet"_word"$wordsize""$suffix" $ERROR_LOG_FILE
}

check_triplet_wordsize(){
	triplet=$1
	wordsize=$2
	# Create a temporary workspace and copy the files to compile
	ROOT_DIR=$CROSSBUILD_OUTPUT/tmp/ecc_root_"$triplet"_"$wordsize"
	mkdir -p $ROOT_DIR
	# Copy necessary source files from the root project
	cp -r $SRC_DIR/src/ $ROOT_DIR/
	cp -r $SRC_DIR/include/ $ROOT_DIR/
	cp $SRC_DIR/common.mk $ROOT_DIR/
	cp $SRC_DIR/Makefile $ROOT_DIR/
	mkdir -p $ROOT_DIR/build
	mkdir -p $CROSSBUILD_OUTPUT/compilation_log
	mkdir -p $CROSSBUILD_OUTPUT/error_log
	COMPILATION_LOG_FILE=$CROSSBUILD_OUTPUT/compilation_log/compilation_log_"$triplet"_"$wordsize"
	ERROR_LOG_FILE=$CROSSBUILD_OUTPUT/error_log/error_log_"$triplet"_"$wordsize"
	# NOTE: for 64 bit triplets, multiarch/crossbuild docker's gcc 4.9 has a bug handling loop unrolling in -O3 and
	# is mistaken in detecting arrays overflows at compilation time
	# See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64277
	# Also, add the "-Wno-pedantic-ms-format" for specific quikrs of mingw with "%lld" (...)
	if [ "$triplet" = "x86_64-w64-mingw32" ] || [ "$triplet" = "aarch64-linux-gnu" ]; then
		extra_lib_cflags="-O2"
		extra_bin_cflags=""
		if [ "$triplet" = "x86_64-w64-mingw32" ] && [ "$wordsize" = "64" ]; then
			extra_lib_cflags=$extra_lib_cflags" -Wno-pedantic-ms-format"
		fi
	# There is also a misbehavior for mingw improperly finding unintialized variables
	# Also, add the "-Wno-pedantic-ms-format" for specific quikrs of mingw with "%lld" (...)
	elif [ "$triplet" = "i686-w64-mingw32" ]; then
		extra_lib_cflags="-Wno-maybe-uninitialized"
		extra_bin_cflags=""
		if [ "$wordsize" = "64" ]; then
			extra_lib_cflags=$extra_lib_cflags" -Wno-pedantic-ms-format"
		fi
        # NOTE: on darwin based clang, some of our options are too recent for the installed
        # llvm ... Hence we remove warnings as errors here
	elif [ "$triplet" = "i386-apple-darwin" ] || [ "$triplet" = "x86_64-apple-darwin" ] || [ "$triplet" = "x86_64h-apple-darwin" ]; then
		extra_lib_cflags="-Wno-error"
		extra_bin_cflags="-Wno-error"
        else
		extra_lib_cflags=""
		extra_bin_cflags=""
	fi
	############## Release compilation
	echo "======== COMPILING RELEASE FOR $triplet / $wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
	# Library, self tests and utils
	docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR -e EXTRA_LIB_CFLAGS="$extra_lib_cflags" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags" multiarch/crossbuild make "$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
	mkdir -p $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"
	check_and_copy $ROOT_DIR/build/ec_self_tests $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize" $ERROR_LOG_FILE
	check_and_copy $ROOT_DIR/build/ec_utils $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_utils_"$triplet"_word"$wordsize" $ERROR_LOG_FILE
	# Examples
	docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/ -w $ROOT_DIR/src/examples -e EXTRA_LIB_CFLAGS="$extra_lib_cflags" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags" multiarch/crossbuild make "$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
	copy_compiled_examples "$ROOT_DIR" "$CROSSBUILD_OUTPUT" "$triplet" "$wordsize" "$ERROR_LOG_FILE" ""
	# Clean
	docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR multiarch/crossbuild make clean
	docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/src/examples -w $ROOT_DIR/src/examples multiarch/crossbuild make clean
	############## Debug compilation
	echo "======== COMPILING DEBUG FOR $triplet / $wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
	############## Release compilation
	# Library, self tests and utils
	docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR -e EXTRA_LIB_CFLAGS="$extra_lib_cflags" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags" multiarch/crossbuild make debug"$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
	mkdir -p $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"
	check_and_copy $ROOT_DIR/build/ec_self_tests $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_debug $ERROR_LOG_FILE
	check_and_copy $ROOT_DIR/build/ec_utils $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_utils_"$triplet"_word"$wordsize"_debug $ERROR_LOG_FILE
	# Examples
	docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/ -w $ROOT_DIR/src/examples -e EXTRA_LIB_CFLAGS="$extra_lib_cflags" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags" multiarch/crossbuild make debug"$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
	copy_compiled_examples "$ROOT_DIR" "$CROSSBUILD_OUTPUT" "$triplet" "$wordsize" "$ERROR_LOG_FILE" "_debug"
	# Clean
	docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR multiarch/crossbuild make clean
	docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/src/examples -w $ROOT_DIR/src/examples multiarch/crossbuild make clean
	echo "===========================================" 2>&1 | tee -a $COMPILATION_LOG_FILE
	# Compile static binaries for everyone except Mac OS (gcc on it does not support -static)
	if [ "$triplet" != "i386-apple-darwin" ] && [ "$triplet" != "x86_64-apple-darwin" ] && [ "$triplet" != "x86_64h-apple-darwin" ]; then
		############## Release compilation with static binaries (for emulation)
		echo "======== COMPILING STATIC RELEASE FOR $triplet / $wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
		# Library, self tests and utils
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR -e EXTRA_LIB_CFLAGS="$extra_lib_cflags" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags" -e BIN_LDFLAGS="-static" multiarch/crossbuild make "$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
		mkdir -p $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"
		check_and_copy $ROOT_DIR/build/ec_self_tests $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_static $ERROR_LOG_FILE
		check_and_copy $ROOT_DIR/build/ec_utils $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_utils_"$triplet"_word"$wordsize"_static $ERROR_LOG_FILE
		# Examples
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/ -w $ROOT_DIR/src/examples -e EXTRA_LIB_CFLAGS="$extra_lib_cflags" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags" -e BIN_LDFLAGS="-static" multiarch/crossbuild make "$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
		copy_compiled_examples "$ROOT_DIR" "$CROSSBUILD_OUTPUT" "$triplet" "$wordsize" "$ERROR_LOG_FILE" "_static"
		# Clean
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR multiarch/crossbuild make clean
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/src/examples -w $ROOT_DIR/src/examples multiarch/crossbuild make clean
		##### 4096 bits case for 64 bit word size only
		###############################################
		if [ "$wordsize" = "64" ]; then
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR multiarch/crossbuild make clean
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/src/examples -w $ROOT_DIR/src/examples multiarch/crossbuild make clean
			# Self tests and utils
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR -e EXTRA_LIB_CFLAGS="$extra_lib_cflags -DUSER_NN_BIT_LEN=4096" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags -DUSER_NN_BIT_LEN=4096" -e BIN_LDFLAGS="-static" multiarch/crossbuild make "$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
			mkdir -p $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"
			check_and_copy $ROOT_DIR/build/ec_self_tests $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_static_4096 $ERROR_LOG_FILE
			check_and_copy $ROOT_DIR/build/ec_utils $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_utils_"$triplet"_word"$wordsize"_static_4096 $ERROR_LOG_FILE
			# Examples
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/ -w $ROOT_DIR/src/examples -e EXTRA_LIB_CFLAGS="$extra_lib_cflags -DUSER_NN_BIT_LEN=4096" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags -DUSER_NN_BIT_LEN=4096" -e BIN_LDFLAGS="-static" multiarch/crossbuild make "$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
			copy_compiled_examples "$ROOT_DIR" "$CROSSBUILD_OUTPUT" "$triplet" "$wordsize" "$ERROR_LOG_FILE" "_static_4096"
			# Clean
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR multiarch/crossbuild make clean
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/src/examples -w $ROOT_DIR/src/examples multiarch/crossbuild make clean
		fi
		############## Debug compilation with static binaries (for emulation)
		echo "======== COMPILING STATIC DEBUG FOR $triplet / $wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
		############## Release compilation with static binaries (for emulation)
		# Self tests and utils
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR -e EXTRA_LIB_CFLAGS="$extra_lib_cflags" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags" -e BIN_LDFLAGS="-static" multiarch/crossbuild make debug"$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
		mkdir -p $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"
		check_and_copy $ROOT_DIR/build/ec_self_tests $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_debug_static $ERROR_LOG_FILE
		check_and_copy $ROOT_DIR/build/ec_utils $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_utils_"$triplet"_word"$wordsize"_debug_static $ERROR_LOG_FILE
		# Examples
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/ -w $ROOT_DIR/src/examples -e EXTRA_LIB_CFLAGS="$extra_lib_cflags" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags" -e BIN_LDFLAGS="-static" multiarch/crossbuild make debug"$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
		copy_compiled_examples "$ROOT_DIR" "$CROSSBUILD_OUTPUT" "$triplet" "$wordsize" "$ERROR_LOG_FILE" "_debug_static"
		# Clean
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR multiarch/crossbuild make clean
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/src/examples -w $ROOT_DIR/src/examples multiarch/crossbuild make clean
		##### 4096 bits case for 64 bit word size only
		###############################################
		if [ "$wordsize" = "64" ]; then
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR multiarch/crossbuild make clean
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/src/examples -w $ROOT_DIR/src/examples multiarch/crossbuild make clean
			# Self tests and utils
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR -e EXTRA_LIB_CFLAGS="$extra_lib_cflags -DUSER_NN_BIT_LEN=4096" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags -DUSER_NN_BIT_LEN=4096" -e BIN_LDFLAGS="-static" multiarch/crossbuild make debug"$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
			mkdir -p $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"
			check_and_copy $ROOT_DIR/build/ec_self_tests $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_self_tests_"$triplet"_word"$wordsize"_debug_static_4096 $ERROR_LOG_FILE
			check_and_copy $ROOT_DIR/build/ec_utils $CROSSBUILD_OUTPUT/"$triplet"/word"$wordsize"/ec_utils_"$triplet"_word"$wordsize"_debug_static_4096 $ERROR_LOG_FILE
			# Examples
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/ -w $ROOT_DIR/src/examples -e EXTRA_LIB_CFLAGS="$extra_lib_cflags -DUSER_NN_BIT_LEN=4096" -e EXTRA_BIN_CFLAGS="$extra_bin_cflags -DUSER_NN_BIT_LEN=4096" -e BIN_LDFLAGS="-static" multiarch/crossbuild make debug"$wordsize" 2>&1 | tee -a $COMPILATION_LOG_FILE
			copy_compiled_examples "$ROOT_DIR" "$CROSSBUILD_OUTPUT" "$triplet" "$wordsize" "$ERROR_LOG_FILE" "_debug_static_4096"
			# Clean
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR -w $ROOT_DIR multiarch/crossbuild make clean
			docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -e CROSS_TRIPLE=$triplet -v $ROOT_DIR:$ROOT_DIR/src/examples -w $ROOT_DIR/src/examples multiarch/crossbuild make clean
		fi
		echo "===========================================" 2>&1 | tee -a $COMPILATION_LOG_FILE
	fi
	# Cleanup compilation stuff
	rm -rf $ROOT_DIR
}


print_help(){
	echo "$0 uses the docker multiarch/crossbuild image to compile libecc on multiple platforms"
	echo "with multiple word sizes (16, 32 and 64). The compilation logs and errors as well as"
	echo "the produced binaries are kept int the 'crossbuild_out' folder."
	echo "Supported platform triplets are:"
	echo "arm-linux-gnueabi / arm-linux-gnueabihf / powerpc64le-linux-gnu / aarch64-linux-gnu /"
	echo "mipsel-linux-gnu / i386-apple-darwin / x86_64-apple-darwin / i686-w64-mingw32 / x86_64-w64-mingw32."
	echo ""
	echo "$0 with no argument will test the compilation for all the triplets and the word sizes."
	echo "  -h: print this help"
	echo "  -triplet: execute the crossbuild only for a given triplet:"
	echo "      $ sh $0 -triplet arm-linux-gnueabi"
	echo "      => This will execute cross-compilation for arm-linux-gnueabi for all the word sizes."
	echo "      $ sh $0 -triplet arm-linux-gnueabi 64"
	echo "      => This will execute cross-compilation for arm-linux-gnueabi only for 64-bit word size."
	echo "  -cpu: will specify the number of tasks used for parallel compilation. The default behaviour is to"
	echo "        use the maximum available CPUs on the machine, but one can reduce th enumber of parallel"
	echo "        tasks with this toggle. Warning: this toggle is not compatible with the -triplet toggle."
}


######### Script main

# Adapt our sources directory depending on the calling
# directory
SRC_DIR=`dirname "$(readlink -f "$0")"`/..
CROSSBUILD_OUTPUT=$SRC_DIR/scripts/crossbuild_out/

# Check for the docker command line
CHECK_DOCKER=$(docker -v)
if [ $? -ne 0 ]; then
	echo "docker is not installed ... Please install it!"
	exit
fi

# Check for docker image multiarch/crossbuild
if [ -z $(docker images -q multiarch/crossbuild) ]
then
	echo "Please install the multiarch/crossbuild docker image:"
	echo "$ docker pull multiarch/crossbuild"
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
	# Clean stuff if this is an explicit call from command line
	if ! [ "$4" = "-automate" ]
	then
		echo "Cleaning before running ..."
		rm -rf $CROSSBUILD_OUTPUT/*
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -v $SRC_DIR:/ecc -w /ecc multiarch/crossbuild make clean
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -v $SRC_DIR:/ecc -w /ecc/src/examples multiarch/crossbuild make clean
		docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -v $SRC_DIR:/ecc -w /ecc/src/arithmetic_tests multiarch/crossbuild make clean
	fi
	# If no specific word size has been given, do all the sizes
	if [ "$3" = "" ]
	then
		for wordsize in 16 32 64;
		do
			check_triplet_wordsize $2 $wordsize
		done
	else
		check_triplet_wordsize $2 $3
	fi
	exit
fi

# Clean
echo "Cleaning before running ..."
rm -rf $CROSSBUILD_OUTPUT/*
docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -v $SRC_DIR:/ecc -w /ecc multiarch/crossbuild make clean
docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -v $SRC_DIR:/ecc -w /ecc/src/examples multiarch/crossbuild make clean
docker run -e VERBOSE=1 -e ASSERT_PRINT="1" -e COMPLETE="$COMPLETE" -e BLINDING="$BLINDING" -e LADDER="$LADDER" -e CRYPTOFUZZ="$CRYPTOFUZZ" --rm -v $SRC_DIR:/ecc -w /ecc/src/arithmetic_tests multiarch/crossbuild make clean

ALL_CHECKS=""
for wordsize in 16 32 64;
do
	for triplet in arm-linux-gnueabi arm-linux-gnueabihf powerpc64le-linux-gnu aarch64-linux-gnu mipsel-linux-gnu i386-apple-darwin x86_64-apple-darwin x86_64h-apple-darwin i686-w64-mingw32 x86_64-w64-mingw32;
	do
		ALL_CHECKS="$ALL_CHECKS\n-triplet $triplet $wordsize -automate"
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

# Check if we had an error, and if yes exit with error
for wordsize in 16 32 64;
do
	for triplet in arm-linux-gnueabi arm-linux-gnueabihf powerpc64le-linux-gnu aarch64-linux-gnu mipsel-linux-gnu i386-apple-darwin x86_64-apple-darwin x86_64h-apple-darwin i686-w64-mingw32 x86_64-w64-mingw32;
	do
		ERROR_LOG_FILE=$CROSSBUILD_OUTPUT/error_log/error_log_"$triplet"_"$wordsize"
		if [ -f "$ERROR_LOG_FILE" ]; then
			echo "!!!!!!! There have been compilation errors for $triplet $wordsize ..."
			exit 255
		fi
	done
done
echo "All compilations went OK!"
