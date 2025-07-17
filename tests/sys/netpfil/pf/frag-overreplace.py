#!/usr/bin/env python3
#
# SPDX-License-Identifier: ISC
#
# Copyright (c) 2012-2021 Alexander Bluhm <bluhm@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

from fragcommon import *

#                               index boundary 4096 |
# |--------------|
#                 ....
#                     |--------------|
#                                              |XXXX-----|
#                                    |--------------|
#                                                   |--------------|

# this should trigger "frag tail overlap %d" and "frag head overlap %d"

def send(src, dst, send_if, recv_if):
	pid = os.getpid()
	eid = pid & 0xffff
	payload = b"ABCDEFGHIJKLMNOP"
	dummy = b"01234567"
	fragsize = 1024
	boundary = 4096
	fragnum = int(boundary / fragsize)
	packet = sp.IP(src=src, dst=dst)/ \
			sp.ICMP(type='echo-request', id=eid)/ \
			(int((boundary + fragsize) / len(payload)) * payload)
	frag = []
	fid = pid & 0xffff

	for i in range(fragnum - 1):
		frag.append(sp.IP(src=src, dst=dst, proto=1, id=fid,
			frag=(i * fragsize) >> 3, flags='MF') /
			bytes(packet)[20 + i * fragsize:20 + (i + 1) * fragsize])
	frag.append(sp.IP(src=src, dst=dst, proto=1, id=fid,
		frag=(boundary - 8) >> 3, flags='MF') /
		(dummy + bytes(packet)[20 + boundary:20 + boundary + 8]))
	frag.append(sp.IP(src=src, dst=dst, proto=1, id=fid,
		frag=(boundary - fragsize) >> 3, flags='MF') /
		bytes(packet)[20 + boundary - fragsize:20 + boundary])
	frag.append(sp.IP(src=src, dst=dst, proto=1, id=fid,
		frag=(boundary) >> 3)/bytes(packet)[20 + boundary:])

	eth=[]
	for f in frag:
		eth.append(sp.Ether() / f)

	if os.fork() == 0:
		time.sleep(1)
		for e in eth:
			sp.sendp(e, iface=send_if)
			time.sleep(0.001)
		os._exit(0)

	ans = sp.sniff(iface=recv_if, timeout=3, filter="")
	for a in ans:
		if a and a.type == sp.ETH_P_IP and \
				a.payload.proto == 1 and \
				a.payload.frag == 0 and \
				sp.icmptypes[a.payload.payload.type] == 'echo-reply':
			id=a.payload.payload.id
			if id != eid:
				print("WRONG ECHO REPLY ID")
				sys.exit(2)
			sys.exit(0)
	print("NO ECHO REPLY")
	sys.exit(1)

if __name__ == '__main__':
	main(send)
