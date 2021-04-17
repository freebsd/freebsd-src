#!/bin/sh

errors=0
umask 0002

DATE="$(date +%s)"
unset LOGBASEDIR
if [ -z "$LOGDIR" ]; then
	LOGBASEDIR=logs
	LOGDIR=$LOGBASEDIR/$DATE
	mkdir -p $LOGDIR
fi
export LOGDIR

if [ -z "$DBFILE" ]; then
    DB=""
else
    DB="-S $DBFILE --commit $(git rev-parse HEAD)"
    if [ -n "$BUILD" ]; then
	DB="$DB -b $BUILD"
    fi
    if [ "$PREFILL_DB" = "y" ] ; then
        DB="$DB --prefill-tests"
    fi
fi

usage()
{
	echo "$0 [-v | --valgrind | valgrind] [-t | --trace | trace]"
	echo "\t[-n <num> | --channels <num>] [-B | --build]"
	echo "\t[-c | --codecov ] [run-tests.py parameters]"
	exit 1
}

unset VALGRIND
unset TRACE
unset TRACE_ARGS
unset RUN_TEST_ARGS
unset BUILD
unset BUILD_ARGS
unset CODECOV
unset VM
while [ "$1" != "" ]; do
	case $1 in
		-v | --valgrind | valgrind)
			shift
			echo "$0: using valgrind"
			VALGRIND=valgrind
			;;
		-t | --trace | trace)
			shift
			echo "$0: using Trace"
			TRACE=trace
			;;
		-n | --channels)
			shift
			NUM_CH=$1
			shift
			echo "$0: using channels=$NUM_CH"
			;;
		-B | --build)
			shift
			echo "$0: build before running tests"
			BUILD=build
			;;
		-c | --codecov)
			shift
			echo "$0: using code coverage"
			CODECOV=lcov
			BUILD_ARGS=-c
			;;
		-h | --help)
			usage
			;;
		-V | --vm)
			shift
			echo "$0: running inside a VM"
			VM=VM
			;;

		*)
			RUN_TEST_ARGS="$RUN_TEST_ARGS$1 "
			shift
			;;
	esac
done

if [ ! -z "$RUN_TEST_ARGS" ]; then
	echo "$0: passing the following args to run-tests.py: $RUN_TEST_ARGS"
fi

unset SUFFIX
if [ ! -z "$BUILD" ]; then
	SUFFIX=-build
fi

if [ ! -z "$VALGRIND" ]; then
	SUFFIX=$SUFFIX-valgrind
fi

if [ ! -z "$TRACE" ]; then
	SUFFIX=$SUFFIX-trace
	TRACE_ARGS="-T"
fi

if [ ! -z "$CODECOV" ]; then
	SUFFIX=$SUFFIX-codecov
fi

if [ ! -z "$BUILD" ]; then
    echo "Building with args=$BUILD_ARGS"
    if ! ./build.sh $BUILD_ARGS; then
	    echo "Failed building components"
	    exit 1
    fi
fi

if ! ./start.sh $VM $VALGRIND $TRACE channels=$NUM_CH; then
	if ! [ -z "$LOGBASEDIR" ] ; then
		echo "Could not start test environment" > $LOGDIR/run
	fi
	exit 1
fi

# Only use sudo if not already root.
if [ "$(id -u)" != 0 ]; then
	SUDO=sudo
else
	SUDO=
fi
${SUDO} ./run-tests.py -D --logdir "$LOGDIR" $TRACE_ARGS -q $DB $RUN_TEST_ARGS || errors=1

./stop.sh

if [ ! -z "$VALGRIND" ] ; then
    failures=`grep "ERROR SUMMARY" $LOGDIR/valgrind-* | grep -v " 0 errors" | wc -l`
    if [ $failures -gt 0 ]; then
	echo "Mark as failed due to valgrind errors"
	errors=1
    fi
fi

if tail -100 $LOGDIR/auth_serv | grep -q MEMLEAK; then
    echo "Mark as failed due to authentication server memory leak"
    errors=1
fi

if [ ! -z "$CODECOV" ] ; then
	lcov -q --capture --directory ../../wpa_supplicant --output-file $LOGDIR/wpas_lcov.info
	genhtml -q $LOGDIR/wpas_lcov.info --output-directory $LOGDIR/wpas_lcov
	lcov -q --capture --directory ../../hostapd --output-file $LOGDIR/hostapd_lcov.info
	genhtml -q $LOGDIR/hostapd_lcov.info --output-directory $LOGDIR/hostapd_lcov
fi

if [ $errors -gt 0 ]; then
    if [ -z $VM ]; then
	tar czf /tmp/hwsim-tests-$DATE-FAILED$SUFFIX.tar.gz $LOGDIR/
    fi
    exit 1
fi

echo "ALL-PASSED"
