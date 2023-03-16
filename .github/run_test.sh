#!/bin/sh

. .github/configs $1

[ -z "${SUDO}" ] || ${SUDO} mkdir -p /var/empty

set -ex

# If we want to test hostbased auth, set up the host for it.
if [ ! -z "$SUDO" ] && [ ! -z "$TEST_SSH_HOSTBASED_AUTH" ]; then
    sshconf=/usr/local/etc
    hostname | $SUDO tee $sshconf/shosts.equiv >/dev/null
    echo "EnableSSHKeysign yes" | $SUDO tee $sshconf/ssh_config >/dev/null
    $SUDO mkdir -p $sshconf
    $SUDO cp -p /etc/ssh/ssh_host*key* $sshconf
    $SUDO make install
    for key in $sshconf/ssh_host*key*.pub; do
        echo `hostname` `cat $key` | \
            $SUDO tee -a $sshconf/ssh_known_hosts >/dev/null
    done
fi

output_failed_logs() {
    for i in regress/failed*.log; do
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
