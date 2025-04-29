#!/usr/bin/env python3

#-
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# This software was developed by Tom Jones <thj@FreeBSD.org> under sponsorship
# from The FreeBSD Foundation

## Extracting logs using the kgdb script
#
# This script extracts tcp black box logs from a kernel core for use with
# tcplog_dumper[1] and readbbr_log[2].
#
# Some system configuration is required to enable black box logs
#
#   [1]: https://github.com/Netflix/tcplog_dumper
#   [2]: https://github.com/Netflix/read_bbrlog
#
# TCP Logs can be extracted from FreeBSD kernel core dumps using the gdb plugin
# provided in the `kgdb` directory. An example usage assuming relevant kernel
# builds and coredumps looks like:
#
#   $ kgdb kernel-debug/kernel.debug vmcore.last
#	Reading symbols from coredump/kernel-debug/kernel.debug...
#
#	Unread portion of the kernel message buffer:
#	KDB: enter: sysctl debug.kdb.enter
#
#	__curthread () at /usr/src/sys/amd64/include/pcpu_aux.h:57
#	57              __asm("movq %%gs:%P1,%0" : "=r" (td) : "n" (offsetof(struct pcpu,
#	(kgdb) source tcplog.py
#	(kgdb) tcplog_dump vnet0
#	processing struct tcpcb *       0xfffff80006e8ca80
#			_t_logstate:    4 _t_logpoint:  0 '\000' t_lognum:      25 t_logsn:     25
#			log written to 0xfffff80006e8ca80_tcp_log.bin
#	processing struct tcpcb *       0xfffff8000ec2b540
#			_t_logstate:    4 _t_logpoint:  0 '\000' t_lognum:      8 t_logsn:      8
#			log written to 0xfffff8000ec2b540_tcp_log.bin
#	processing struct tcpcb *       0xfffff80006bd9540
#			no logs
#	processing struct tcpcb *       0xfffff80006bd9a80
#			no logs
#	processing struct tcpcb *       0xfffff8001d837540
#			no logs
#	processing struct tcpcb *       0xfffff8001d837000
#			no logs
#
#	processed 1 vnets, dumped 2 logs
#			0xfffff80006e8ca80_tcp_log.bin 0xfffff8000ec2b540_tcp_log.bin
#
#
# The generated files can be given to tcplog_dumper to generate pcaps like so:
#
#	$ tcplog_dumper -s -f 0xfffff80006e8ca80_tcp_log.bin
#

import struct

TLB_FLAG_RXBUF =        0x0001  #/* Includes receive buffer info */
TLB_FLAG_TXBUF =        0x0002  #/* Includes send buffer info */
TLB_FLAG_HDR =          0x0004  #/* Includes a TCP header */
TLB_FLAG_VERBOSE =      0x0008  #/* Includes function/line numbers */
TLB_FLAG_STACKINFO =    0x0010  #/* Includes stack-specific info */

TCP_LOG_BUF_VER =       9   # from netinet/tcp_log_buf.h
TCP_LOG_DEV_TYPE_BBR =  1   # from dev/tcp_log/tcp_log_dev.h

TCP_LOG_ID_LEN  =       64
TCP_LOG_TAG_LEN =       32
TCP_LOG_REASON_LEN =    32

AF_INET =               2
AF_INET6 =              28

INC_ISIPV6 =            0x01

class TCPLogDump(gdb.Command):

    def __init__(self):
        super(TCPLogDump, self).__init__(
            "tcplog_dump", gdb.COMMAND_USER
        )

    def dump_tcpcb(self, tcpcb):
        if tcpcb['t_lognum'] == 0:
            print("processing {}\t{}\n\tno logs".format(tcpcb.type, tcpcb))
            return
        else:
            print("processing {}\t{}".format(tcpcb.type, tcpcb))

        print("\t_t_logstate:\t{} _t_logpoint:\t{} t_lognum:\t{} t_logsn:\t{}".format(
            tcpcb['_t_logstate'], tcpcb['_t_logpoint'], tcpcb['t_lognum'], tcpcb['t_logsn']))

        eaddr = (tcpcb['t_logs']['stqh_first'])
        log_buf = bytes()
        while eaddr != 0:
            log_buf += self.print_tcplog_entry(eaddr)
            eaddr = eaddr.dereference()['tlm_queue']['stqe_next']

        if log_buf:
            filename = "{}_tcp_log.bin".format(tcpcb)

            with open(filename, "wb") as f:
                f.write(self.format_header(tcpcb, eaddr, len(log_buf)))
                f.write(log_buf)
            self.logfiles_dumped.append(filename)
            print("\tlog written to {}".format(filename))

    # tcpcb, entry address, length of data for header
    def format_header(self, tcpcb, eaddr, datalen):
        # Get a handle we can use to read memory
        inf = gdb.inferiors()[0]    # in a coredump this should always be safe

        # Add the common header
        hdrlen = gdb.parse_and_eval("sizeof(struct tcp_log_header)")
        hdr = struct.pack("=llq", TCP_LOG_BUF_VER, TCP_LOG_DEV_TYPE_BBR, hdrlen+datalen)

        inp = tcpcb.cast(gdb.lookup_type("struct inpcb").pointer())

        # Add entry->tldl_ie
        bufaddr = gdb.parse_and_eval(
            "&(((struct inpcb *){})->inp_inc.inc_ie)".format(tcpcb))
        length = gdb.parse_and_eval("sizeof(struct in_endpoints)")
        hdr += inf.read_memory(bufaddr, length).tobytes()

        # Add boot time
        hdr += struct.pack("=16x") # BOOTTIME

        # Add id, tag and reason as UNKNOWN

        unknown = bytes("UNKNOWN", "ascii")

        hdr += struct.pack("={}s{}s{}s"
               .format(TCP_LOG_ID_LEN, TCP_LOG_TAG_LEN, TCP_LOG_REASON_LEN),
               unknown, unknown, unknown
        )

        # Add entry->tldl_af
        if inp['inp_inc']['inc_flags'] & INC_ISIPV6:
            hdr += struct.pack("=b", AF_INET6)
        else:
            hdr += struct.pack("=b", AF_INET)

        hdr += struct.pack("=7x") # pad[7]

        if len(hdr) != hdrlen:
            print("header len {} bytes NOT CORRECT should be {}".format(len(hdr), hdrlen))

        return hdr

    def print_tcplog_entry(self, eaddr):
        # implement tcp_log_logs_to_buf
        entry = eaddr.dereference()

        # If header is present copy out entire buffer
        # otherwise copy just to the start of the header.
        if entry['tlm_buf']['tlb_eventflags'] & TLB_FLAG_HDR:
            length = gdb.parse_and_eval("sizeof(struct tcp_log_buffer)")
        else:
            length = gdb.parse_and_eval("&((struct tcp_log_buffer *) 0)->tlb_th")

        bufaddr = gdb.parse_and_eval("&(((struct tcp_log_mem *){})->tlm_buf)".format(eaddr))

        # Get a handle we can use to read memory
        inf = gdb.inferiors()[0]    # in a coredump this should always be safe
        buf_mem = inf.read_memory(bufaddr, length).tobytes()

        # If needed copy out a header size worth of 0 bytes
        # this was a simple expression untiil gdb got involved.
        if not entry['tlm_buf']['tlb_eventflags'] & TLB_FLAG_HDR:
            buf_mem += bytes([0 for b
                in range(
                    gdb.parse_and_eval("sizeof(struct tcp_log_buffer) - {}".format(length))
                )
            ])

        # If verbose is set:
        if entry['tlm_buf']['tlb_eventflags'] & TLB_FLAG_VERBOSE:
            bufaddr = gdb.parse_and_eval("&(((struct tcp_log_mem *){})->tlm_v)".format(eaddr))
            length = gdb.parse_and_eval("sizeof(struct tcp_log_verbose)")
            buf_mem += inf.read_memory(bufaddr, length).tobytes()

        return buf_mem

    def dump_vnet(self, vnet):
        # This is the general access pattern for something in a vnet.
        cmd = "(struct inpcbinfo*)((((struct vnet *) {} )->vnet_data_base) + (uintptr_t )&vnet_entry_tcbinfo)".format(vnet)
        ti = gdb.parse_and_eval(cmd)

        # Get the inplist head (struct inpcb *)(struct inpcbinfo*)({})->ipi_listhead
        inplist = ti['ipi_listhead']
        self.walk_inplist(inplist)

    def walk_inplist(self, inplist):
        inp = inplist['clh_first']
        while inp != 0:
            self.dump_tcpcb(inp.cast(gdb.lookup_type("struct tcpcb").pointer()))
            inp = inp['inp_list']['cle_next']

    def walk_vnets(self, vnet):
        vnets = []
        while vnet != 0:
            vnets.append(vnet)
            vnet = vnet['vnet_le']['le_next']
        return vnets

    def complete(self, text, word):
        return gdb.COMPLETE_SYMBOL

    def invoke(self, args, from_tty):
        if not args:
            self.usage()
            return

        self.logfiles_dumped = []

        node = gdb.parse_and_eval(args)

        # If we are passed vnet0 pull out the first vnet, it is always there.
        if str(node.type) == "struct vnet_list_head *":
            print("finding start of the vnet list and continuing")
            node = node["lh_first"]

        if str(node.type) == "struct vnet *":
            vnets = self.walk_vnets(node)
            for vnet in vnets:
                self.dump_vnet(vnet)

            print("\nprocessed {} vnets, dumped {} logs\n\t{}"
                .format(len(vnets), len(self.logfiles_dumped), " ".join(self.logfiles_dumped)))
        elif str(node.type) == "struct inpcbinfo *":
            inplist = node['ipi_listhead']
            self.walk_inplist(inplist)

            print("\ndumped {} logs\n\t{}"
                .format(len(self.logfiles_dumped), " ".join(self.logfiles_dumped)))
        elif str(node.type) == "struct tcpcb *":
            self.dump_tcpcb(node)
        else:
            self.usage()

        return

    def usage(self):
        print("tcplog_dump <address ish>")
        print("Locate tcp_log_buffers and write them to a file")
        print("Address can be one of:")
        print("\tvnet list head (i.e. vnet0)")
        print("\tvnet directly")
        print("\tinpcbinfo")
        print("\ttcpcb")
        print("\nIf given anything other than a struct tcpcb *, will try and walk all available members")
        print("that can be found")
        print("\n")
        print("logs will be written to files in cwd in the format:")
        print("\t\t `%p_tcp_log.bin` struct tcpcb *")
        print("\t\t existing files will be stomped on")
        print("\nexamples:\n")
        print("\t(kgdb) tcplog_dump vnet0")
        print("\t(kgdb) tcplog_dump (struct inpcbinfo *)V_tcbinfo # on a non-vnet kernel (maybe, untested)")
        print("\t(kgdb) tcplog_dump (struct tcpcb *)0xfffff80006e8ca80")
        print("\t\twill result in a file called: 0xfffff80006e8ca80_tcp_log.bin\n\n")

        return

TCPLogDump()

