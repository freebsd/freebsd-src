#!/bin/sh
#       $OpenBSD: sshd-log-wrapper.sh,v 1.3 2013/04/07 02:16:03 dtucker Exp $
#       Placed in the Public Domain.
#
# simple wrapper for sshd proxy mode to catch stderr output
# sh sshd-log-wrapper.sh /path/to/sshd /path/to/logfile

sshd=$1
log=$2
shift
shift

exec $sshd -E$log $@
