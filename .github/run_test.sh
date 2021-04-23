#!/usr/bin/env bash

. .github/configs $1 $2

[ -z "${SUDO}" ] || ${SUDO} mkdir -p /var/empty

set -ex

if [ -z "${LTESTS}" ]; then
    make ${TEST_TARGET} SKIP_LTESTS="${SKIP_LTESTS}"
    result=$?
else
    make ${TEST_TARGET} SKIP_LTESTS="${SKIP_LTESTS}" LTESTS="${LTESTS}"
    result=$?
fi

if [ ! -z "${SSHD_CONFOPTS}" ]; then
    echo "rerunning tests with TEST_SSH_SSHD_CONFOPTS='${SSHD_CONFOPTS}'"
    make t-exec TEST_SSH_SSHD_CONFOPTS="${SSHD_CONFOPTS}"
    result2=$?
    if [ "${result2}" -ne 0 ]; then
        result="${result2}"
    fi
fi

if [ "$result" -ne "0" ]; then
    for i in regress/failed*; do
        echo -------------------------------------------------------------------------
        echo LOGFILE $i
        cat $i
        echo -------------------------------------------------------------------------
    done
fi
