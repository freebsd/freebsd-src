/*-
 * Copyright (c) 1997 John D. Polstra
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#ifndef _MACHINE_SETDEFS_H_
#define _MACHINE_SETDEFS_H_

/*
 * This file contains declarations for each linker set that can possibly
 * be included in the system.  Every linker set must have an entry in this
 * file, regardless of whether it happens to be configured into a given
 * kernel.
 *
 * These declarations are unused for a.out, but they are needed for an
 * ELF kernel.  ELF does not directly support linker sets, so they are
 * simulated by creating a separate section for each set.  This header
 * is included by two source files: setdef0.c at the beginning of the
 * link (just after locore.s), and setdef1.c at the end of the link.
 * The DEFINE_SET macro is defined differently in these two souces, so
 * that it emits the leading count word for each set in setdef0.c, and
 * the trailing NULL pointer for each set in setdef1.c.
 */

DEFINE_SET(db_show_cmd_set);
DEFINE_SET(domain_set);
DEFINE_SET(eisadriver_set);
DEFINE_SET(execsw_set);
DEFINE_SET(netisr_set);
DEFINE_SET(pcibus_set);
DEFINE_SET(pcidevice_set);
DEFINE_SET(sysctl_);
DEFINE_SET(sysctl__debug);
DEFINE_SET(sysctl__hw);
DEFINE_SET(sysctl__kern);
DEFINE_SET(sysctl__kern_ipc);
DEFINE_SET(sysctl__kern_ntp_pll);
DEFINE_SET(sysctl__kern_proc);
DEFINE_SET(sysctl__kern_proc_pgrp);
DEFINE_SET(sysctl__kern_proc_pid);
DEFINE_SET(sysctl__kern_proc_ruid);
DEFINE_SET(sysctl__kern_proc_tty);
DEFINE_SET(sysctl__kern_proc_uid);
DEFINE_SET(sysctl__machdep);
DEFINE_SET(sysctl__net);
DEFINE_SET(sysctl__net_inet);
DEFINE_SET(sysctl__net_inet_div);
DEFINE_SET(sysctl__net_inet_icmp);
DEFINE_SET(sysctl__net_inet_igmp);
DEFINE_SET(sysctl__net_inet_ip);
DEFINE_SET(sysctl__net_inet_ip_fw);
DEFINE_SET(sysctl__net_inet_ip_portrange);
DEFINE_SET(sysctl__net_inet_raw);
DEFINE_SET(sysctl__net_inet_tcp);
DEFINE_SET(sysctl__net_inet_udp);
DEFINE_SET(sysctl__net_ipx);
DEFINE_SET(sysctl__net_ipx_error);
DEFINE_SET(sysctl__net_ipx_ipx);
DEFINE_SET(sysctl__net_ipx_spx);
DEFINE_SET(sysctl__net_link);
DEFINE_SET(sysctl__net_link_ether);
DEFINE_SET(sysctl__net_link_ether_inet);
DEFINE_SET(sysctl__net_link_generic);
DEFINE_SET(sysctl__net_link_generic_ifdata);
DEFINE_SET(sysctl__net_link_generic_system);
DEFINE_SET(sysctl__net_local);
DEFINE_SET(sysctl__net_local_dgram);
DEFINE_SET(sysctl__net_local_stream);
DEFINE_SET(sysctl__net_routetable);
DEFINE_SET(sysctl__sysctl);
DEFINE_SET(sysctl__sysctl_name);
DEFINE_SET(sysctl__sysctl_next);
DEFINE_SET(sysctl__sysctl_oidfmt);
DEFINE_SET(sysctl__user);
DEFINE_SET(sysctl__vfs);
DEFINE_SET(sysctl__vfs_ffs);
DEFINE_SET(sysctl__vfs_generic);
DEFINE_SET(sysctl__vfs_nfs);
DEFINE_SET(sysctl__vm);
DEFINE_SET(sysinit_set);
DEFINE_SET(vfs_opv_descs_);
DEFINE_SET(vfs_set);

#endif /* !_MACHINE_SETDEFS_H_ */
