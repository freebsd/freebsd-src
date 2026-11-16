#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Avinash Duduskar <avinash.duduskar@gmail.com>
#
# Run pfctl as authpf(8) does: open /dev/pf and a Netlink socket as root,
# drop to nobody, exec pfctl with -p /dev/fd/N and, given -l, -l M.

import os
import pwd
import socket
import sys

AF_NETLINK = 38         # sys/socket.h; not exported by the socket module
NETLINK_GENERIC = 16    # netlink/netlink.h


def main():
    args = sys.argv[1:]
    adopt = False
    if args and args[0] == "-l":
        adopt = True
        args = args[1:]

    dev = os.open("/dev/pf", os.O_RDWR)
    os.set_inheritable(dev, True)
    nl = socket.socket(AF_NETLINK, socket.SOCK_RAW, NETLINK_GENERIC)
    os.set_inheritable(nl.fileno(), True)

    pw = pwd.getpwnam("nobody")
    os.setgroups([pw.pw_gid])
    os.setgid(pw.pw_gid)
    os.setuid(pw.pw_uid)

    argv = ["pfctl", "-p", "/dev/fd/%d" % dev]
    if adopt:
        argv += ["-l", str(nl.fileno())]
    os.execv("/sbin/pfctl", argv + args)


if __name__ == "__main__":
    main()
