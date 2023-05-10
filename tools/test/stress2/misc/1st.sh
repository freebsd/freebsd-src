#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# stress2 config test:
# Check that diskimage and RUNDIR has not been clobbered in `hostname`.
# Variables must be set like this: var=${var:-value}

export diskimage=dummy
export RUNDIR=dummy
. ../default.cfg

[ "`sysctl -in debug.vmem_check`" = "1" ] &&
    echo "debug.vmem_check must be set to 0"
[ "`sysctl -in debug.vmmap_check`" = "0" ] &&
    echo "debug.vmmap_check must be set to 1"

if [ "$diskimage" != "dummy" ]; then
	echo "FATAL: diskimage was overwritten with \"$diskimage\""
	exit 1
fi
if [ "$RUNDIR" != "dummy" ]; then
	echo "FATAL: RUNDIR was overwritten with \"$RUNDIR\""
	exit 1
fi
if [ "`dirname $mntpoint`" != "/" ]; then
	echo "FATAL: mntpoint \"$mntpoint\" must be a root directory"
	exit 1
fi
[ -z "`which ruby 2>/dev/null`" ] && echo "Consider installing ruby"
[ -z "`type mke2fs 2>/dev/null`" ] && echo "Consider installing e2fsprogs"
[ -z "`type mkisofs 2>/dev/null`" ] && echo "Consider installing cdrtools"
[ -z "`type mDNSNetMonitor 2>/dev/null`" ] && echo "Consider installing mDNSResponder"
[ ! -x /usr/local/lib/libmill.so ] && echo "Consider installing libmill"

# Random sanity checks

df -k $(dirname $diskimage) | tail -1 | awk '{print $4}' |
    grep -Eq '^[0-9]+$' || { echo FATAL; df -k $(dirname $diskimage); }

grep -Eq "^discard" /etc/inetd.conf ||
    echo "Discard is not enabled in /etc/inetd.conf"
pgrep -Sq inetd || echo "inetd is not running"

[ `sysctl -n kern.maxvnodes` -le 2000 ] &&
    echo "FATAL kern.maxvnodes is too small"

exit 0
