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
# $FreeBSD$
#

import argparse
import scapy.all as sp
import socket
import sys
import frag6.sniffer as Sniffer
from time import sleep

def check_icmp6_error_dst_unreach_noport(args, packet):
	ip6 = packet.getlayer(sp.IPv6)
	if not ip6:
		return False
	oip6 = sp.IPv6(src=args.src[0], dst=args.to[0])
	if ip6.dst != oip6.src:
		return False
	icmp6 = packet.getlayer(sp.ICMPv6DestUnreach)
	if not icmp6:
		return False
	# ICMP6_DST_UNREACH_NOPORT 4
	if icmp6.code != 4:
		return False
	# Should we check the payload as well?
	# We are running in a very isolated environment and nothing else
	# should trigger an ICMPv6 Dest Unreach / Port Unreach so leave it.
	#icmp6.display()
	return True

def check_icmp6_error_paramprob_header(args, packet):
	ip6 = packet.getlayer(sp.IPv6)
	if not ip6:
		return False
	oip6 = sp.IPv6(src=args.src[0], dst=args.to[0])
	if ip6.dst != oip6.src:
		return False
	icmp6 = packet.getlayer(sp.ICMPv6ParamProblem)
	if not icmp6:
		return False
	# ICMP6_PARAMPROB_HEADER 0
	if icmp6.code != 0:
		return False
	# Should we check the payload as well?
	# We are running in a very isolated environment and nothing else
	# should trigger an ICMPv6 Param Prob so leave it.
	#icmp6.display()
	return True

def check_tcp_rst(args, packet):
	ip6 = packet.getlayer(sp.IPv6)
	if not ip6:
		return False
	oip6 = sp.IPv6(src=args.src[0], dst=args.to[0])
	if ip6.dst != oip6.src:
		return False
	tcp = packet.getlayer(sp.TCP)
	if not tcp:
		return False
	# Is TCP RST?
	if tcp.flags & 0x04:
		#tcp.display()
		return True
	return False

def addExt(ext, h):
	if h is None:
		return ext
	if ext is None:
		ext = h
	else:
		ext = ext / h
	return ext

def getExtHdrs(args):
	ext = None

	# XXX-TODO Try to put them in an order which could make sense
	# in real life packets and according to the RFCs.
	if args.hbh:
		hbh = sp.IPv6ExtHdrHopByHop(options = \
		    sp.PadN(optdata="\x00\x00\x00\x00\x00\x00"))
		ext = addExt(ext, hbh)

	if args.rh:
		rh = sp.IPv6ExtHdrRouting(type = 0)
		ext = addExt(ext, rh)

	if args.frag6:
		frag6 = sp.IPv6ExtHdrFragment(offset=0, m=0, id=0x1234)
		ext = addExt(ext, frag6)

	if args.esp:
		# XXX TODO
		esp = None
		ext = addExt(ext, esp)

	if args.ah:
		# XXX TODO
		ah = None
		ext = addExt(ext, ah)

	if args.dest:
		dest = sp.IPv6ExtHdrDestOpt(options = \
		    sp.PadN(optdata="\x00\x00\x00\x00\x00\x00"))
		ext = addExt(ext, dest)

	if args.mobi:
		# XXX TODO
		mobi = None
		ext = addExt(ext, mobi)

	if args.hip:
		# XXX TODO
		hip = None
		ext = addExt(ext, hip)

	if args.shim6:
		# XXX TODO
		shim6 = None
		ext = addExt(ext, shim6)

	if args.proto253:
		# XXX TODO
		tft = None
		ext = addExt(ext, tft)

	if args.proto254:
		# XXX TODO
		tff = None
		ext = addExt(ext, tff)

	if args.hbhbad:
		hbhbad = sp.IPv6ExtHdrHopByHop(options = \
		    sp.PadN(optdata="\x00\x00\x00\x00\x00\x00"))
		ext = addExt(ext, hbhbad)

	return ext

def main():
	parser = argparse.ArgumentParser("exthdr.py",
		description="IPv6 extension header test tool")
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
	# Extension Headers
	# See https://www.iana.org/assignments/ipv6-parameters/ipv6-parameters.xhtml
	parser.add_argument('--hbh',
		required=False, action='store_true',
		help='Add IPv6 Hop-by-Hop Option')
	parser.add_argument('--hbhbad',
		required=False, action='store_true',
		help='Add IPv6 Hop-by-Hop Option at an invalid position')
	parser.add_argument('--rh',
		required=False, action='store_true',
		help='Add Routing Header for IPv6')
	parser.add_argument('--frag6',
		required=False, action='store_true',
		help='Add Fragment Header for IPv6')
	parser.add_argument('--esp',
		required=False, action='store_true',
		help='Add Encapsulating Security Payload')
	parser.add_argument('--ah',
		required=False, action='store_true',
		help='Add Authentication Header')
	parser.add_argument('--dest',
		required=False, action='store_true',
		help='Add Destination Options for IPv6')
	parser.add_argument('--mobi',
		required=False, action='store_true',
		help='Add Mobility Header')
	parser.add_argument('--hip',
		required=False, action='store_true',
		help='Add Host Identity Protocol')
	parser.add_argument('--shim6',
		required=False, action='store_true',
		help='Add Shim6 Protocol')
	parser.add_argument('--proto253',
		required=False, action='store_true',
		help='Use for experimentation and testing (253)')
	parser.add_argument('--proto254',
		required=False, action='store_true',
		help='Use for experimentation and testing (254)')

	args = parser.parse_args()

	if args.hbhbad:
		ok = 0
	else:
		ok = 1

	########################################################################
	#
	# Send IPv6 packets with one or more extension headers (combinations
	# mmight not always make sense depending what user tells us).
	# We are trying to cover the basic loop and passing mbufs on
	# and making sure m_pullup() works.
	# Try for at least UDP and TCP upper layer payloads.
	#
	# Expectations: no panics
	# We are not testing for any other outcome here.
	#
	data = "6" * 88
	udp = sp.UDP(dport=3456, sport=6543) / data
	tcp = sp.TCP(dport=4567, sport=7654)
	ip6 = sp.Ether() / sp.IPv6(src=args.src[0], dst=args.to[0])
	for ulp in [ udp, tcp ]:
		ext = getExtHdrs(args)
		if ext is not None:
			pkt = ip6 / ext / ulp
		else:
			pkt = ip6 / ulp
		if args.debug :
			pkt.display()
		if not ok:
			sc = check_icmp6_error_paramprob_header;
		elif ulp == udp:
			sc = check_icmp6_error_dst_unreach_noport;
		elif ulp == tcp:
			sc = check_tcp_rst;
		else:
			sys.exit(2)
		# Start sniffing on recvif
		sniffer = Sniffer.Sniffer(args, sc)
		sp.sendp(pkt, iface=args.sendif[0], verbose=False)
		sleep(0.10)
		sniffer.setEnd()
		sniffer.join()
		if not sniffer.foundCorrectPacket:
			sys.exit(not ok)

	sys.exit(0)

if __name__ == '__main__':
	main()
