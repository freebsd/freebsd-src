#!/bin/sh
#-
# Copyright (c) 2010 iXsystems, Inc.  All rights reserved.
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

# Create our device listing
SYSDISK=$(sysctl -n kern.disks)

# Now loop through these devices, and list the disk drives
for i in ${SYSDISK}
do

  # Get the current device
  DEV="${i}"

  # Make sure we don't find any cd devices
  case "${DEV}" in
     acd[0-9]*|cd[0-9]*|scd[0-9]*) continue ;;
  esac

  # Check the dmesg output for some more info about this device
  NEWLINE=$(dmesg | sed -n "s/^$DEV: .*<\(.*\)>.*$/ <\1>/p" | head -n 1)
  if [ -z "$NEWLINE" ]; then
    NEWLINE=" <Unknown Device>"
  fi

  # Save the disk list
  if [ ! -z "$DLIST" ]
  then
    DLIST="\n${DLIST}"
  fi

  DLIST="${DEV}:${NEWLINE}${DLIST}"

done

# Echo out the found line
echo -e "$DLIST" | sort
