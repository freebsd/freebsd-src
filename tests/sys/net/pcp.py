#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Rubicon Communications, LLC (Netgate).
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

import argparse
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sp
import sys
import os
curdir = os.path.dirname(os.path.realpath(__file__))
netpfil_common = curdir + "/../netpfil/common"
sys.path.append(netpfil_common)
from sniffer import Sniffer

def check_pcp(args, packet):
	vlan = packet.getlayer(sp.Dot1Q)

	if vlan is None:
		return False

	if not packet.getlayer(sp.BOOTP):
		return False

	if vlan.prio == int(args.expect_pcp[0]):
		return True

	return False

def main():
	parser = argparse.ArgumentParser("pcp.py",
		description="PCP test tool")
	parser.add_argument('--recvif', nargs=1,
		required=True,
		help='The interface where to look for packets to check')
	parser.add_argument('--expect-pcp', nargs=1,
		help='The expected PCP value on VLAN packets')

	args = parser.parse_args()

	sniffer = Sniffer(args, check_pcp, recvif=args.recvif[0], timeout=20)

	sniffer.join()

	if sniffer.foundCorrectPacket:
		sys.exit(0)

	sys.exit(1)

if __name__ == '__main__':
	main()
