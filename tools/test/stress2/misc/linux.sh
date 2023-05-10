#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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

# Bug 230144 - Linux emulator does not work on Ryzen / Epic processors
# No problems seen.

[ -x /compat/linux/bin/date ] || exit 0
kldstat | grep -q linux.ko && exit 0

set -e
kldload linux.ko
mount -t linprocfs linprocfs /compat/linux/proc
mount -t linsysfs linsysfs /compat/linux/sys
mount -t tmpfs -o rw,mode=1777 tmpfs /compat/linux/dev/shm
[ `uname -m` = amd64 ] && kldload linux64.ko
set +e
[ -x /compat/linux/bin/bash ] &&
    /compat/linux/bin/bash -c "/compat/linux/bin/date"

[ `uname -m` = amd64 ] && kldunload linux64.ko
umount /compat/linux/dev/shm
kldstat | grep -q tmpfs.ko && kldunload tmpfs.ko
umount /compat/linux/sys
kldunload linsysfs.ko
umount /compat/linux/proc
kldunload linprocfs.ko
kldunload linux.ko

exit 0
