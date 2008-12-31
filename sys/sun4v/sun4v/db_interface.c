/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * $FreeBSD: src/sys/sun4v/sun4v/db_interface.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <sys/param.h> 
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/ktr.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <machine/cpu.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

#include <machine/atomic.h>
#include <machine/setjmp.h>

int
db_read_bytes(vm_offset_t addr, size_t size, char *data)
{
	jmp_buf jb;
	void *prev_jb;
	char *src;
	int ret;

	prev_jb = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		src = (char *)addr;
		while (size-- > 0)
			*data++ = *src++;
	}
	(void)kdb_jmpbuf(prev_jb);
	return (ret);
}

int
db_write_bytes(vm_offset_t addr, size_t size, char *data)
{
	jmp_buf jb;
	void *prev_jb;
	char *dst;
	int ret;

	prev_jb = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		dst = (char *)addr;
		while (size-- > 0)
			*dst++ = *data++;
	}
	(void)kdb_jmpbuf(prev_jb);
	return (ret);
}

void
db_show_mdpcpu(struct pcpu *pc)
{
}

DB_COMMAND(reboot, db_reboot)
{
	cpu_reset();
}

DB_COMMAND(halt, db_halt)
{
	cpu_halt();
}
