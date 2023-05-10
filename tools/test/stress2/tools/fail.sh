#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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

# List tests returning a non zero exit code from today and yesterday.

log=/tmp/stress2.d/fail
out=/tmp/fail.sh.out
[ -r $log ] || exit 1

egrep "^(`date +%Y%m%d`|`date -v-1d +%Y%m%d`)" $log | while read l; do
	name=`echo $l | sed 's/.* //'`
	grep -q "^$name" $0 && continue
	echo $name
done | sort -u | while read f; do
	grep $f $log | tail -1
done | sort -n > $out
if [ -s $out ]; then
	echo
	echo "Summary: Tests which failed within the last two days."
	while read l; do
		n=` echo $l | sed 's/.* //'`
		c=`grep -c $n $log`
		[ $c -eq 1 ] &&
		    echo "$l (new)" || echo "$l"
	done < $out
fi
rm -f $out
exit 0

# List of known failures:

credleak.sh			20170321 Known lockd issue
db.sh				20170323 ls stalls for more than 90 secs
dev3.sh				20170323 pts memory leak
gnop5.sh			20170905 mount: Invalid sectorsize 16384
graid1_7.sh			20170430 FAIL Remove component gptid/02b9...
graid1_8.sh			20170512 Known: do not run fsck
mountu.sh			20170321 Known NFS problem
nfs13.sh			20170323 tmpfs using options triggers this
nfs15lockd.sh			20170330 unmount of /mnt failed: Device busy
nfs15lockd3.sh			20170330 unmount of /mnt failed: Device busy
nfssillyrename.sh		20170321 Known problem
rename11.sh			20170329 Too many links seen with SU.
swappedout.sh			20170321 Known to fail
tmpfs18.sh			20170323 tmpfs -o issue
tvnlru.sh			20170329 Known performance issue
umountf2.sh			20170323 umount hangs
vm_fault_dontneed.sh		20170321 Recognition of sequential access
