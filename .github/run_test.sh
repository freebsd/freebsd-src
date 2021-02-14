#!/usr/bin/env bash

TARGETS=$@

TEST_TARGET="tests"
LTESTS=""  # all tests by default

set -ex

for TARGET in $TARGETS; do
    case $TARGET in
    --without-openssl)
        # When built without OpenSSL we can't do the file-based RSA key tests.
        TEST_TARGET=t-exec
        ;;
    esac
done

if [ -z "$LTESTS" ]; then
    make $TEST_TARGET
    result=$?
else
    make $TEST_TARGET LTESTS="$LTESTS"
    result=$?
fi

if [ "$result" -ne "0" ]; then
    for i in regress/failed*; do
        echo -------------------------------------------------------------------------
        echo LOGFILE $i
        cat $i
        echo -------------------------------------------------------------------------
    done
fi
