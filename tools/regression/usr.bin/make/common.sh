#!/bin/sh
#
# Common code used run regression tests for usr.bin/make.
#
# $FreeBSD$

#
# Output a message and exit with an error.
#
fatal()
{
	echo "fatal: $*" >/dev/stderr
	exit 1
}

#
# Check whether the working directory exists - it must.
#
ensure_workdir()
{
	if [ ! -d ${WORK_DIR} ] ; then
		fatal "working directory ${WORK_DIR} does not exist."
	fi
}

#
# Make sure all tests have been run
#
ensure_run()
{
	if [ -z "${TEST_N}" ] ; then
		TEST_N=1
	fi

	FAIL=
	N=1
	while [ ${N} -le ${TEST_N} ] ; do
		eval skip=\${TEST_${N}_SKIP}
		if [ -z "${skip}" ] ; then
			if [ ! -f ${OUTPUT_DIR}/status.${N} -o \
			     ! -f ${OUTPUT_DIR}/stdout.${N} -o \
			     ! -f ${OUTPUT_DIR}/stderr.${N} ] ; then
				echo "Test ${SUBDIR}/${N} no yet run"
				FAIL=yes
			fi
		fi
		N=$((N + 1))
	done

	if [ ! -z "${FAIL}" ] ; then
		exit 1
	fi
}

#
# Output usage messsage.
#
print_usage()
{
	echo "Usage: sh -v -m <path> -w <dir> $0 command(s)"
	echo " setup	- setup working directory"
	echo " run	- run the tests"
	echo " show	- show test results"
	echo " compare	- compare actual and expected results"
	echo " diff	- diff actual and expected results"
	echo " reset	- reset the test to its initial state"
	echo " clean	- delete working and output directory"
	echo " test	- setup + run + compare"
	echo " prove	- setup + run + compare + clean"
	echo " desc	- print short description"
	echo " update	- update the expected results with the current results"
	echo " help	- show this information"
}

#
# Common function for setup and reset.
#
common_setup()
{
	#
	# If a Makefile exists in the source directory - copy it over
	#
	if [ -e Makefile -a ! -e ${WORK_DIR}/Makefile ] ; then
		cp Makefile ${WORK_DIR}/Makefile
	fi

	#
	# If the TEST_MAKE_DIRS variable is set, create those directories
	#
	set -- ${TEST_MAKE_DIRS}
	while [ $# -ne 0 ] ; do
		if [ ! -d ${WORK_DIR}/${1} ] ; then
			mkdir -p -m ${2} ${WORK_DIR}/${1}
		else
			chmod ${2} ${WORK_DIR}/${1}
		fi
		shift ; shift
	done

	#
	# If the TEST_COPY_FILES variable is set, copy those files over to
	# the working directory. The value is assumed to be pairs of
	# filenames and modes.
	#
	set -- ${TEST_COPY_FILES}
	while [ $# -ne 0 ] ; do
		if [ ! -e ${WORK_DIR}/${1} ] ; then
			cp ${1} ${WORK_DIR}/${1}
		fi
		chmod ${2} ${WORK_DIR}/${1}
		shift ; shift
	done

	#
	# If the TEST_TOUCH variable is set, it is taken to be a list
	# of pairs of filenames and arguments to touch(1). The arguments
	# to touch must be surrounded by single quotes if there are more
	# than one argument.
	#
	eval set -- ${TEST_TOUCH}
	while [ $# -ne 0 ] ; do
		eval touch ${2} ${WORK_DIR}/${1}
		shift ; shift
	done

	#
	# Now create links
	#
	eval set -- ${TEST_LINKS}
	while [ $# -ne 0 ] ; do
		eval ln ${WORK_DIR}/${1} ${WORK_DIR}/${2}
		shift ; shift
	done
}

#
# Setup the test. This creates the working and output directories and
# populates it with files. If there is a setup_test() function - call it.
#
eval_setup()
{
	#
	# Check whether the working directory exists. If it does exit
	# fatally so that we don't clobber a test the user is working on.
	#
	if [ -d ${WORK_DIR} ] ; then
		fatal "working directory ${WORK_DIR} already exists."
	fi

	#
	# Now create it and the output directory
	#
	mkdir -p ${WORK_DIR}
	rm -rf ${OUTPUT_DIR}
	mkdir -p ${OUTPUT_DIR}

	#
	# Common stuff
	#
	common_setup

	#
	# Now after all execute the user's setup function if it exists.
	#
	setup_test
}

#
# Default setup_test function does nothing. This may be overriden by
# the test.
#
setup_test()
{
}

#
# Reset the test. Here we need to rely on information from the test.
# We executed the same steps as in the setup, by try not to clobber existing
# files.
# All files and directories that are listed on the TEST_CLEAN_FILES
# variable are removed. Then the TEST_TOUCH list is executed and finally
# the reset_test() function called if it exists.
#
eval_reset()
{
	ensure_workdir

	#
	# Clean the output directory
	#
	rm -rf ${OUTPUT_DIR}/*

	#
	# Common stuff
	#
	common_setup

	#
	# Remove files.
	#
	for f in ${TEST_CLEAN_FILES} ; do
		rm -rf ${WORK_DIR}/${f}
	done

	#
	# Execute test's function
	#
	reset_test
}

#
# Default reset_test function does nothing. This may be overriden by
# the test.
#
reset_test()
{
}

#
# Clean the test. This simply removes the working and output directories.
#
eval_clean()
{
	rm -rf ${WORK_DIR}
	rm -rf ${OUTPUT_DIR}
}

#
# Run the test.
#
eval_run()
{
	ensure_workdir

	if [ -z "${TEST_N}" ] ; then
		TEST_N=1
	fi

	N=1
	while [ ${N} -le ${TEST_N} ] ; do
		eval skip=\${TEST_${N}_SKIP}
		if [ -z "${skip}" ] ; then
			( cd ${WORK_DIR} ;
			  exec 1>${OUTPUT_DIR}/stdout.${N} 2>${OUTPUT_DIR}/stderr.${N}
			  run_test ${N}
			  echo $? >${OUTPUT_DIR}/status.${N}
			)
		fi
		N=$((N + 1))
	done
}

#
# Default run_test() function.  It can be replaced by the
# user specified regression test. The argument to this function is
# the test number.
#
run_test()
{
	eval args=\${TEST_${1}-test${1}}
        ${MAKE_PROG} $args
}

#
# Show test results.
#
eval_show()
{
	ensure_workdir

	if [ -z "${TEST_N}" ] ; then
		TEST_N=1
	fi

	N=1
	while [ ${N} -le ${TEST_N} ] ; do
		eval skip=\${TEST_${N}_SKIP}
		if [ -z "${skip}" ] ; then
			echo "=== Test ${N} Status =================="
			cat ${OUTPUT_DIR}/status.${N}
			echo ".......... Stdout .................."
			cat ${OUTPUT_DIR}/stdout.${N}
			echo ".......... Stderr .................."
			cat ${OUTPUT_DIR}/stderr.${N}
		fi
		N=$((N + 1))
	done
}

#
# Compare results with expected results
#
eval_compare()
{
	ensure_workdir
	ensure_run

	if [ -z "${TEST_N}" ] ; then
		TEST_N=1
	fi

	echo "1..${TEST_N}"
	N=1
	while [ ${N} -le ${TEST_N} ] ; do
		eval skip=\${TEST_${N}_SKIP}
		if [ -z "${skip}" ] ; then
			FAIL=
			do_compare stdout ${N} || FAIL="${FAIL}stdout "
			do_compare stderr ${N} || FAIL="${FAIL}stderr "
			do_compare status ${N} || FAIL="${FAIL}status "
			if [ ! -z "$FAIL" ]; then
				echo "not ok ${N} ${SUBDIR}/${N} # reason: ${FAIL}"
			else
				echo "ok ${N} ${SUBDIR}/${N}"
			fi
		else
			echo "ok ${N} ${SUBDIR}/${N} # skip: ${skip}"
		fi
		N=$((N + 1))
	done
}

#
# Check if the test result is the same as the expected result.
#
# $1	Input file
# $2	Test number
#
do_compare()
{
	local EXPECTED RESULT
	EXPECTED="expected.$1.$2"
	RESULT="${OUTPUT_DIR}/$1.$2"

	if [ -f $EXPECTED ]; then
		diff -q $EXPECTED $RESULT 1>/dev/null 2>/dev/null
		return $?
	else
		return 1	# FAIL
	fi
}

#
# Diff current and expected results
#
eval_diff()
{
	ensure_workdir
	ensure_run

	if [ -z "${TEST_N}" ] ; then
		TEST_N=1
	fi

	N=1
	while [ ${N} -le ${TEST_N} ] ; do
		eval skip=\${TEST_${N}_SKIP}
		if [ -z "${skip}" ] ; then
			FAIL=
			do_diff stdout ${N}
			do_diff stderr ${N}
			do_diff status ${N}
		fi
		N=$((N + 1))
	done
}

#
# Check if the test result is the same as the expected result.
#
# $1	Input file
# $2	Test number
#
do_diff()
{
	local EXPECTED RESULT
	EXPECTED="expected.$1.$2"
	RESULT="${OUTPUT_DIR}/$1.$2"

	echo diff -u $EXPECTED $RESULT
	if [ -f $EXPECTED ]; then
		diff -u $EXPECTED $RESULT
	else
		echo "${EXPECTED} does not exist"
	fi
}

#
# Update expected results
#
eval_update()
{
	ensure_workdir
	ensure_run

	if [ -z "${TEST_N}" ] ; then
		TEST_N=1
	fi

	FAIL=
	N=1
	while [ ${N} -le ${TEST_N} ] ; do
		eval skip=\${TEST_${N}_SKIP}
		if [ -z "${skip}" ] ; then
			cp ${OUTPUT_DIR}/stdout.${N} expected.stdout.${N}
			cp ${OUTPUT_DIR}/stderr.${N} expected.stderr.${N}
			cp ${OUTPUT_DIR}/status.${N} expected.status.${N}
		fi
		N=$((N + 1))
	done
}

#
# Print description
#
eval_desc()
{
	echo "${SUBDIR}: ${DESC}"
}

#
# Run the test
#
eval_test()
{
	eval_setup
	eval_run
	eval_compare
}

#
# Run the test for prove(1)
#
eval_prove()
{
	eval_setup
	eval_run
	eval_compare
	eval_clean
}

#
# Main function. Execute the command(s) on the command line.
#
eval_cmd()
{
	if [ $# -eq 0 ] ; then
		# if no arguments given default to 'prove'
		set -- prove
	fi

	for i
	do
		case $i in

		setup | run | compare | diff | clean | reset | show | \
		test | prove | desc | update)
			eval eval_$i
			;;
		* | help)
			print_usage
			;;
		esac
	done
}

##############################################################################
#
# Main code
#

#
# Parse command line arguments.
#
args=`getopt m:w:v $*`
if [ $? != 0 ]; then
	echo 'Usage: ...'
	exit 2
fi
set -- $args
for i; do
	case "$i" in
	-m)
		MAKE_PROG="$2"
		shift
		shift
		;;
	-w)
		WORK_BASE="$2"
		shift
		shift
		;;
	-v)
		VERBOSE=1
		shift
		;;
	--)
		shift
		break
		;;
	esac
done

#
# Determine our sub-directory. Argh.
#
SRC_DIR=`pwd`
SRC_BASE=`while [ ! -f common.sh ] ; do cd .. ; done ; pwd`
SUBDIR=`echo ${SRC_DIR} | sed "s@${SRC_BASE}/@@"`

#
# Construct working directory
#
WORK_BASE=${WORK_BASE:-"/tmp/$USER.make.test"}
WORK_DIR=${WORK_BASE}/${SUBDIR}
OUTPUT_DIR=${WORK_DIR}.OUTPUT

#
# Make to use
#
MAKE_PROG=${MAKE_PROG:-/usr/bin/make}
