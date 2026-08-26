#!/bin/sh

#
# Copyright (c) 2026 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# "nullfs mount over a file" scenario

set -u
prog=$(basename "$0" .sh)

file=/tmp/$prog.file
mp=/tmp/$prog.mp
touch $file $mp

s=0
mount_nullfs $file $mp || exit 1
echo a > $mp
cmp -s $file $mp ||
    { echo "Files differ"; ls -l $file $mp; s=1; }
umount $mp
cmp -s $file $mp &&
    { echo "Files are equal"; ls -l $file $mp; s=2; }

rm -f $file $mp
exit $s
