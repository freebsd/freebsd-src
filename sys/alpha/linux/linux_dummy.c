/*-
 * Copyright (c) 2000 Andrew Gallatin
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <alpha/linux/linux.h>
#include <alpha/linux/linux_proto.h>
#include <compat/linux/linux_util.h>

DUMMY(ptrace);
DUMMY(syslog);
DUMMY(vhangup);
DUMMY(swapoff);
DUMMY(sysinfo);
DUMMY(adjtimex);
DUMMY(create_module);
DUMMY(init_module);
DUMMY(delete_module);
DUMMY(get_kernel_syms);
DUMMY(quotactl);
DUMMY(bdflush);
DUMMY(sysfs);
DUMMY(setfsuid);
DUMMY(setfsgid);
DUMMY(query_module);
DUMMY(nfsservctl);
DUMMY(prctl);
DUMMY(rt_sigpending);
DUMMY(rt_sigtimedwait);
DUMMY(rt_sigqueueinfo);
DUMMY(capget);
DUMMY(capset);
DUMMY(sendfile);

DUMMY(getdtablesize);
DUMMY(gethostname);
DUMMY(getpagesize);
DUMMY(mincore);
DUMMY(old_adjtimex);
DUMMY(pciconfig_iobase);
DUMMY(pciconfig_read);
DUMMY(pciconfig_write);
DUMMY(pivot_root);
DUMMY(recvmsg);
DUMMY(sendmsg);
DUMMY(sethae);
DUMMY(sigaltstack);
DUMMY(sigpending);
DUMMY(socketpair);
DUMMY(utimes);

