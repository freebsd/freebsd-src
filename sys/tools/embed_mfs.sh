#!/bin/sh
#
# Copyright (C) 2008 The FreeBSD Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
# Embed the MFS image into the kernel body (expects space reserved via 
# MD_ROOT_SIZE)
#
# $1: kernel filename
# $2: MFS image filename
#

mfs_size=`stat -f '%z' $2 2> /dev/null`
# If we can't determine MFS image size - bail.
[ -z ${mfs_size} ] && echo "Can't determine MFS image size" && exit 1

sec_info=`${CROSS_BINUTILS_PREFIX}objdump -h $1 2> /dev/null | grep " oldmfs "`
# If we can't find the mfs section within the given kernel - bail.
[ -z "${sec_info}" ] && echo "Can't locate mfs section within kernel" && exit 1

sec_size=`echo ${sec_info} | awk '{printf("%d", "0x" $3)}' 2> /dev/null`
sec_start=`echo ${sec_info} | awk '{printf("%d", "0x" $6)}' 2> /dev/null`

# If the mfs section size is smaller than the mfs image - bail.
[ ${sec_size} -lt ${mfs_size} ] && echo "MFS image too large" && exit 1

# Dump the mfs image into the mfs section
dd if=$2 ibs=8192 of=$1 obs=${sec_start} oseek=1 conv=notrunc 2> /dev/null && \
    echo "MFS image embedded into kernel" && exit 0
