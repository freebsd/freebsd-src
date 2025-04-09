#!/bin/sh

. .github/configs $1

[ -z "${SUDO}" ] || ${SUDO} mkdir -p /var/empty

set -ex

# If we want to test hostbased auth, set up the host for it.
if [ ! -z "$SUDO" ] && [ ! -z "$TEST_SSH_HOSTBASED_AUTH" ]; then
    sshconf=/usr/local/etc
    $SUDO mkdir -p "${sshconf}"
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

env=""
if [ ! -z "${SUDO}" ]; then
    env="${env} SUDO=${SUDO}"
fi
if [ ! -z "${TCMALLOC_STACKTRACE_METHOD}" ]; then
    env="${env} TCMALLOC_STACKTRACE_METHOD=${TCMALLOC_STACKTRACE_METHOD}"
fi
if [ ! -z "${TEST_SSH_SSHD_ENV}" ]; then
    env="${env} TEST_SSH_SSHD_ENV=${TEST_SSH_SSHD_ENV}"
fi
if [ ! -z "${env}" ]; then
    env="env${env}"
fi

if [ -z "${LTESTS}" ]; then
    ${env} make ${TEST_TARGET} SKIP_LTESTS="${SKIP_LTESTS}"
else
    ${env} make ${TEST_TARGET} SKIP_LTESTS="${SKIP_LTESTS}" LTESTS="${LTESTS}"
fi

if [ ! -z "${SSHD_CONFOPTS}" ]; then
    echo "rerunning t-exec with TEST_SSH_SSHD_CONFOPTS='${SSHD_CONFOPTS}'"
    if [ -z "${LTESTS}" ]; then
        ${env} make t-exec SKIP_LTESTS="${SKIP_LTESTS}" TEST_SSH_SSHD_CONFOPTS="${SSHD_CONFOPTS}"
    else
        ${env} make t-exec SKIP_LTESTS="${SKIP_LTESTS}" LTESTS="${LTESTS}" TEST_SSH_SSHD_CONFOPTS="${SSHD_CONFOPTS}"
    fi
fi
