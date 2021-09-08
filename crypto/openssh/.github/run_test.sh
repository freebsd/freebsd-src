#!/bin/sh

. .github/configs $1

[ -z "${SUDO}" ] || ${SUDO} mkdir -p /var/empty

set -ex

output_failed_logs() {
    for i in regress/failed*; do
        if [ -f "$i" ]; then
            echo -------------------------------------------------------------------------
            echo LOGFILE $i
            cat $i
            echo -------------------------------------------------------------------------
        fi
    done
}
trap output_failed_logs 0

if [ -z "${LTESTS}" ]; then
    make ${TEST_TARGET} SKIP_LTESTS="${SKIP_LTESTS}"
else
    make ${TEST_TARGET} SKIP_LTESTS="${SKIP_LTESTS}" LTESTS="${LTESTS}"
fi

if [ ! -z "${SSHD_CONFOPTS}" ]; then
    echo "rerunning t-exec with TEST_SSH_SSHD_CONFOPTS='${SSHD_CONFOPTS}'"
    if [ -z "${LTESTS}" ]; then
        make t-exec SKIP_LTESTS="${SKIP_LTESTS}" TEST_SSH_SSHD_CONFOPTS="${SSHD_CONFOPTS}"
    else
        make t-exec SKIP_LTESTS="${SKIP_LTESTS}" LTESTS="${LTESTS}" TEST_SSH_SSHD_CONFOPTS="${SSHD_CONFOPTS}"
    fi
fi
