#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# sysctl(3) regression test by using pstat and vmstat

for i in `jot 100`; do
	pstat -T | grep -q files || echo "pstat -T Failed"
	pstat -f | grep -q inode || echo "pstat -f Failed"
	pstat -s | grep -q /dev/ || echo "pstat -s Failed"
	pstat -t | grep -q tty   || echo "pstat -t Failed"

	[ `vmstat -P | wc -l` -eq 3 ] || echo "vmstat -P Failed"
	vmstat -f | grep -q forks     || echo "vmstat -f Failed"
	vmstat -i | grep -q cpu       || echo "vmstat -i Failed"
	vmstat -m | grep -q lockf     || echo "vmstat -m Failed"
	vmstat -s | grep -q inter     || echo "vmstat -s Failed"
	vmstat -z | grep -q dinode    || echo "vmstat -z Failed"
done
exit 0
