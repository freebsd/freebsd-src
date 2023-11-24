#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# A brif test of proccontrol(1). No problems seen.

[ -x /usr/bin/proccontrol ] || exit 0

sleep 60 & pid=$!
proccontrol -m aslr    -s disable sleep 1
proccontrol -m aslr    -s enable  sleep 1
proccontrol -m aslr               sleep 1
proccontrol -m aslr    -s disable -p $pid
proccontrol -m aslr    -s enable  -p $pid
proccontrol -m aslr    -q         -p $pid

proccontrol -m trace   -s disable sleep 1
proccontrol -m trace   -s enable  sleep 1
proccontrol -m trace              sleep 1
proccontrol -m trace   -q         -p $pid

proccontrol -m trapcap -s disable sleep 1
proccontrol -m trapcap -s enable  sleep 1
proccontrol -m trapcap            sleep 1
proccontrol -m trapcap -s disable -p $pid
proccontrol -m trapcap -s enable  -p $pid
proccontrol -m trapcap -q         -p $pid
kill $pid
wait
