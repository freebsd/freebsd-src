#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
# All rights reserved.
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

# Test using the OpenSolaris libmicro benchmark
# Has shown page fault with the cascade_lockf test

if [ $# -eq 0 ]; then
	. ../default.cfg

	[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

	[ -x /usr/local/bin/libmicro-bench ] ||
	    { echo "ports/benchmarks/libmicro is not installed"; exit 0; }

	[ `id -un` = $testuser ] &&
	    { echo "\$testuser is identical to current id"; exit 1; }

	rm -f /tmp/libmicro.log
	trap "rm -rf /var/tmp/libmicro.[0-9]*" 0
	su $testuser -c "$0 x"
	echo ""
else
	/usr/local/bin/libmicro-bench > /tmp/libmicro.log &
	# Temp. work-around for hanging "c_lockf_10" test.
	sleep 60
	kill 0
	wait
fi
