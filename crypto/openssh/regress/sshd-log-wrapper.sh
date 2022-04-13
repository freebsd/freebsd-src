#!/bin/sh
#       $OpenBSD: sshd-log-wrapper.sh,v 1.5 2022/01/04 08:38:53 dtucker Exp $
#       Placed in the Public Domain.
#
# simple wrapper for sshd proxy mode to catch stderr output
# sh sshd-log-wrapper.sh /path/to/logfile /path/to/sshd [args...]

log=$1
shift

echo "Executing: $@" >>$log
exec "$@" -E$log
