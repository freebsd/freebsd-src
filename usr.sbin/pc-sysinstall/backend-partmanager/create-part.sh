#!/bin/sh
#-
# Copyright (c) 2010 iX Systems, Inc.  All rights reserved.
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

# Query a disk for partitions and display them
#############################

. ${PROGDIR}/backend/functions.sh

if [ -z "${1}" ] ; then
  echo "Error: No disk specified!"
  exit 1
fi

if [ -z "${2}" ] ; then
  echo "Error: No size specified!"
  exit 1
fi

if [ ! -e "/dev/${1}" ] ; then
  echo "Error: Disk /dev/${1} does not exist!"
  exit 1
fi

DISK="${1}"
MB="${2}"

TOTALBLOCKS="`expr $MB \* 2048`"


# Lets figure out what number this slice will be
LASTSLICE="`fdisk -s /dev/${DISK} 2>/dev/null | grep -v ${DISK} | grep ':' | tail -n 1 | cut -d ':' -f 1 | tr -s '\t' ' ' | tr -d ' '`"
if [ -z "${LASTSLICE}" ] ; then
  LASTSLICE="1"
else
  LASTSLICE="`expr $LASTSLICE + 1`"
fi

if [ ${LASTSLICE} -gt "4" ] ; then
  echo "Error: FreeBSD MBR setups can only have a max of 4 slices"
  exit 1
fi


SLICENUM="${LASTSLICE}"

# Lets get the starting block
if [ "${SLICENUM}" = "1" ] ; then
  STARTBLOCK="63"
else
  # Lets figure out where the prior slice ends
  checkslice="`expr ${SLICENUM} - 1`"

  # Get starting block of this slice
  fdisk -s /dev/${DISK} | grep -v "${DISK}:" | grep "${checkslice}:" | tr -s " " >${TMPDIR}/pfdisk
  pstartblock="`cat ${TMPDIR}/pfdisk | cut -d ' ' -f 3`"
  psize="`cat ${TMPDIR}/pfdisk | cut -d ' ' -f 4`"
  STARTBLOCK="`expr ${pstartblock} + ${psize}`"
fi


# If this is an empty disk, see if we need to create a new MBR scheme for it
gpart show ${DISK} >/dev/null 2>/dev/null
if [ "$?" != "0" -a "${SLICENUM}" = "1" ] ; then
 gpart create -s mbr ${DISK}
fi

gpart add -b ${STARTBLOCK} -s ${TOTALBLOCKS} -t freebsd -i ${SLICENUM} ${DISK}
exit "$?"
