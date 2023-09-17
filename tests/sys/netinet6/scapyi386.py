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

def main():
	parser = argparse.ArgumentParser("scapyi386.py",
		description="IPv6 Ethernet Dest MAC test")
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
	# A test case to check that IPv6 packets are sent with a proper
	# (resolved) Ethernet Destination MAC address instead of the BCAST one.
	# This was needed as test cases did not work properly on i386 due to a
	# scapy BPF parsing bug. (See PR 239380 and duplicates).
	#
	bcmac = sp.Ether(dst="ff:ff:ff:ff:ff:ff").dst
	data = "6" * 88
	pkt = sp.Ether() / \
		sp.IPv6(src=args.src[0], dst=args.to[0]) / \
		sp.UDP(dport=3456, sport=6543) / \
		data
	sp.sendp(pkt, iface=args.sendif[0], verbose=False)

	eth = pkt.getlayer(sp.Ether)
	if eth is None:
		print("No Ether in packet")
		pkt.display()
		sys.exit(1)
	if eth.dst == bcmac:
		print("Broadcast dMAC on packet")
		eth.display()
		sys.exit(1)

	sys.exit(0)

if __name__ == '__main__':
	main()
