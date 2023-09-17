#! /usr/bin/env python3
# Used to inject various malformed packets

import errno
import logging
import subprocess
import sys

logging.getLogger("scapy").setLevel(logging.CRITICAL)

from scapy.all import IP, ICMP, IPOption
import scapy.layers.all
from scapy.layers.inet import ICMPEcho_am
from scapy.layers.tuntap import TunTapInterface

SRC_ADDR = "192.0.2.14"
DST_ADDR = "192.0.2.15"

mode = sys.argv[1]
ip = None

# fill opts with nop (0x01)
opts = b''
for x in range(40):
    opts += b'\x01'


# Create and configure a tun interface with an RFC5737 nonrouteable address
create_proc = subprocess.run(
    args=["ifconfig", "tun", "create"],
    capture_output=True,
    check=True,
    text=True)
iface = create_proc.stdout.strip()
tun = TunTapInterface(iface)
with open("tun.txt", "w") as f:
    f.write(iface)
subprocess.run(["ifconfig", tun.iface, "up"])
subprocess.run(["ifconfig", tun.iface, SRC_ADDR, DST_ADDR])

ping = subprocess.Popen(
        args=["/sbin/ping", "-v", "-c1", "-t1", DST_ADDR],
        text=True
)
# Wait for /sbin/ping to ping us
echo_req = tun.recv()

# Construct the response packet
if mode == "opts":
    # Sending reply with IP options
    echo_reply = IP(
        dst=SRC_ADDR,
        src=DST_ADDR,
        options=IPOption(opts)
    )/ICMP(type=0, code=0, id=echo_req.payload.id)/echo_req.payload.payload
elif mode == "pip":
    # packet in packet (inner has options)

    inner = IP(
        dst=SRC_ADDR,
        src=DST_ADDR,
        options=IPOption(opts)
    )/ICMP(type=0, code=0, id=echo_req.payload.id)/echo_req.payload.payload
    outer = IP(
        dst=SRC_ADDR,
        src=DST_ADDR
    )/ICMP(type=3, code=1)  # host unreach

    echo_reply = outer/inner
elif mode == "reply":
    # Sending normal echo reply
    echo_reply = IP(
        dst=SRC_ADDR,
        src=DST_ADDR,
    )/ICMP(type=0, code=0, id=echo_req.payload.id)/echo_req.payload.payload
else:
    print("unknown mode {}".format(mode))
    exit(1)

tun.send(echo_reply)
outs, errs = ping.communicate()

sys.exit(ping.returncode)
