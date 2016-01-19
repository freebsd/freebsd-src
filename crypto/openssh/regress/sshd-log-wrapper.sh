#!/bin/sh
#       $OpenBSD: sshd-log-wrapper.sh,v 1.3 2013/04/07 02:16:03 dtucker Exp $
#       Placed in the Public Domain.
#
# simple wrapper for sshd proxy mode to catch stderr output
# sh sshd-log-wrapper.sh /path/to/logfile /path/to/sshd [args...]

log=$1
shift

exec "$@" -E$log
