#!/usr/bin/env python
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Netflix, Inc.
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
#

import argparse
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sp
import socket
import sys
from time import sleep

def main():
	parser = argparse.ArgumentParser("frag6.py",
		description="IPv6 fragementation test tool")
	parser.add_argument('--sendif', nargs=1,
		required=True,
		help='The interface through which the packet will be sent')
	parser.add_argument('--recvif', nargs=1,
		required=True,
		help='The interface on which to check for the packet')
	parser.add_argument('--src', nargs=1,
		required=True,
		help='The source IP address')
	parser.add_argument('--to', nargs=1,
		required=True,
		help='The destination IP address')
	parser.add_argument('--debug',
		required=False, action='store_true',
		help='Enable test debugging')

	args = parser.parse_args()


	########################################################################
	#
	# Sysctl set to accept maximum 3 segments on a fragmented packet.
	# The 4th packet will flush the entire q6.
	#
	# A:  4 Discarded.
	# R:  Silence (statistics only) no ICMPv6 as we skip the off=0 segment.
	#
	data = "66666666"
	for i in range(4):
		foffset=16 + (i * (0x100 + (int)(16 / 8)))
		ip6f01 = sp.Ether() / \
			sp.IPv6(src=args.src[0], dst=args.to[0]) / \
			sp.IPv6ExtHdrFragment(offset=foffset, m=1, id=15) / \
			sp.UDP(dport=3456, sport=6543) / \
			data
		if args.debug :
			ip6f01.display()
		sp.sendp(ip6f01, iface=args.sendif[0], verbose=False)


	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # ##
	#
	# Sysctl set to accept maximum 3 segments on a fragmented packet.
	# The 4th packet will flush the entire q6.
	# This time we play proper offset/length games on the packets in order
	# to trigger the 2nd test case, with the last packet still having m=1.
	#
	# A:  4 Discarded.
	# R:  ICMPv6 timeout expired.
	#
	data = "66666666"
	for i in range(4):
		foffset=(i * (int)(16 / 8))
		ip6f01 = sp.Ether() / \
			sp.IPv6(src=args.src[0], dst=args.to[0]) / \
			sp.IPv6ExtHdrFragment(offset=foffset, m=1, id=0x1501) / \
			sp.UDP(dport=3456, sport=6543) / \
			data
		if args.debug :
			ip6f01.display()
		sp.sendp(ip6f01, iface=args.sendif[0], verbose=False)

	sys.exit(0)

if __name__ == '__main__':
	main()
