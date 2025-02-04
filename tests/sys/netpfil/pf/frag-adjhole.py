#!/usr/bin/env python3
#
# Copyright (c) 2025 Alexander Bluhm <bluhm@openbsd.org>

from fragcommon import *

# |--------|
#          |--------|
#      |-------|
#                   |----|

def send(src, dst, send_if, recv_if):
	pid = os.getpid()
	eid = pid & 0xffff
	payload = b"ABCDEFGHIJKLMNOP" * 2
	packet = sp.IP(src=src, dst=dst)/ \
	    sp.ICMP(type='echo-request', id=eid) / payload
	frag = []
	fid = pid & 0xffff
	frag.append(sp.IP(src=src, dst=dst, proto=1, id=fid,
	    flags='MF') / bytes(packet)[20:36])
	frag.append(sp.IP(src=src, dst=dst, proto=1, id=fid,
	    frag=2, flags='MF') / bytes(packet)[36:52])
	frag.append(sp.IP(src=src, dst=dst, proto=1, id=fid,
	    frag=1, flags='MF') / bytes(packet)[28:44])
	frag.append(sp.IP(src=src, dst=dst, proto=1, id=fid,
	    frag=4) / bytes(packet)[52:60])
	eth=[]
	for f in frag:
		eth.append(sp.Ether()/f)
	if os.fork() == 0:
		time.sleep(1)
		sp.sendp(eth, iface=send_if)
		os._exit(0)

	ans = sp.sniff(iface=recv_if, timeout=3, filter=
	    "ip and src " + dst + " and dst " + src + " and icmp")
	for a in ans:
		if a and a.type == sp.ETH_P_IP and \
		    a.payload.proto == 1 and \
		    a.payload.frag == 0 and a.payload.flags == 0 and \
		    sp.icmptypes[a.payload.payload.type] == 'echo-reply':
			id = a.payload.payload.id
			print("id=%#x" % (id))
			if id != eid:
				print("WRONG ECHO REPLY ID")
				exit(2)
			data = a.payload.payload.payload.load
			print("payload=%s" % (data))
			if data == payload:
				exit(0)
			print("PAYLOAD!=%s" % (payload))
			exit(1)
	print("NO ECHO REPLY")
	exit(2)

if __name__ == '__main__':
	main(send)
