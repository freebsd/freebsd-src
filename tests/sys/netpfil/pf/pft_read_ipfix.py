#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright © 2023. Rubicon Communications, LLC (Netgate). All Rights Reserved.
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

import argparse
import logging
logging.getLogger("scapy").setLevel(logging.CRITICAL)
import scapy.all as sp

def receive(recvif, recvport):
	pkts = sp.sniff(iface=recvif, timeout=65)

	if len(pkts) == 0:
		print("No data")
		return

	for pkt in pkts:
		udp = pkt.getlayer(sp.UDP)
		if not udp:
			continue

		if udp.dport != recvport:
			continue

		hdr = pkt.getlayer(sp.NetflowHeader)

		if hdr.version == 5:
			v5hdr = pkt.getlayer(sp.NetflowHeaderV5)
			out=""
			for i in range(1, v5hdr.count + 1):
				r = pkt.getlayer(sp.NetflowRecordV5, nb=i)
				out = "%s,proto=%d,src=%s,dst=%s,srcport=%d,dstport=%d" % (out, r.prot, r.src, r.dst, r.srcport, r.dstport)
			print("v=%d,count=%d%s" % (hdr.version, v5hdr.count, out))
		elif hdr.version == 10:
			print("v=10")
			return

def main():
	parser = argparse.ArgumentParser("pft_read_ipfix.py",
	    description="IPFix test tool")
	parser.add_argument('--recvif', nargs=1,
	    required=True,
	    help='The interface on which to look for packets')
	parser.add_argument('--port', nargs=1,
	    required=True,
	    help='The port number')

	args = parser.parse_args()

	receive(args.recvif[0], int(args.port[0]))

if __name__ == '__main__':
	main()

