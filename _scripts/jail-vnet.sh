#!/bin/sh
#-
# Copyright (c) 2016 The FreeBSD Foundation
# All rights reserved.
#
# This software was developed by Björn Zeeb under
# the sponsorship from the FreeBSD Foundation.
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
# $FreeBSD$
#

set -e

xid=`id -u`
case ${xid} in
0)	;;
*)	echo "Run as superuser"
	exit 1
	;;
esac

sh jail-vnet-setup-host.sh

#sysctl security.jail.callout_debug=0 || true
#sysctl sysctl vm.uma_zone_nofree_ignore=0 || true
#kenv net.inet.ip.flowtable.enable=0

sh loop.sh cleanup-epairs.sh &
#sh loop.sh jail-vnet-netperf.sh &
sh loop.sh jail-vnet-start-stop.sh &
sh loop.sh jail-vnet-test-10.sh &
sh loop.sh jail-vnet-test-20.sh &
sh loop.sh jail-vnet-test-21.sh &
sh loop.sh jail-vnet-test-30.sh &
sh loop.sh jail-vnet-test.sh &
sh loop.sh conductor/tests/http/run-as-shell-not-conductor.sh &
sh loop.sh conductor/tests/http/run-as-shell-not-conductor-pf.sh &
sh loop.sh jail-start-lmr-ipfw.sh &

sh loop.sh stop-netperf.sh &
sh jail-random-stop.sh

# end
