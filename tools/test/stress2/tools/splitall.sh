#/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# A way to split all.sh across different hosts.
# For example "./splitall.sh 2 3" will run the second third of the tests.

# Split the test list up in n parts and test one of them.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ $# -ne 2 ] && echo "Usage $0 <part number> <parts>" && exit 1

pno=$1
parts=$2
[ $pno -lt 1 -o $pno -gt $parts -o $parts -lt 1 ] &&
    { echo "<part number> must be between 1 and <parts> ($parts)"; exit 1; }

cd ../misc
exclude=`cat all.exclude $alllocal 2>/dev/null | sed '/^#/d' |
    grep "\.sh" | awk '{print $1}'`

list=$(echo `ls *.sh` | sed  "s/all\.sh//; s/cleanup\.sh//")

lst=""
for i in $list; do
	echo $exclude | grep -qw $i && continue
	lst="$lst $i"
done
n=`echo $lst | wc -w`
(cd /tmp; echo $lst | tr ' ' '\n' | split -d -l $((n / parts + 1)) - str)
part=`printf "/tmp/str%02d" $((pno - 1))`
plist=`cat $part | tr '\n' ' '`
rm -f /tmp/str0?
echo "./all.sh -onc $plist"
sleep 10
./all.sh -onc $plist
