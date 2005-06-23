/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

static struct kerneldumpheader kdh;

void
dumpsys(struct dumperinfo *di)
{
	off_t dumplo;
	vm_offset_t a, addr;
	u_int count, left, u;
	void *va;
	int i, mb;
	int c;

	printf("Dumping %ld MB\n", Maxmem / (1024*1024 / PAGE_SIZE));

	/* Fill in the kernel dump header */
	strcpy(kdh.magic, KERNELDUMPMAGIC);
	strcpy(kdh.architecture, "amd64");
	kdh.version = htod32(KERNELDUMPVERSION);
	kdh.architectureversion = htod32(KERNELDUMP_AMD64_VERSION);
	kdh.dumplength = htod64(Maxmem * (off_t)PAGE_SIZE);
	kdh.dumptime = htod64(time_second);
	kdh.blocksize = htod32(di->blocksize);
	strncpy(kdh.hostname, hostname, sizeof kdh.hostname);
	strncpy(kdh.versionstring, version, sizeof kdh.versionstring);
	if (panicstr != NULL)
		strncpy(kdh.panicstring, panicstr, sizeof kdh.panicstring);
	kdh.parity = kerneldump_parity(&kdh);

	/*
	 * Check if we will have enough room to save the coredump.
	 * The partition size needed is the sum of:
	 * Memory to save + header + trailer + Room to leave untouched
	 * at partition head. (an arbitrary amount).
	 */
	if (di->mediasize <  
	    Maxmem * (off_t)PAGE_SIZE + sizeof kdh * 2 + 64*1024) {
		printf("\nDump failed. Partition too small.\n");
		return;
	}
	dumplo = di->mediaoffset + di->mediasize - Maxmem * (off_t)PAGE_SIZE;
	dumplo -= sizeof kdh * 2;
	i = di->dumper(di->priv, &kdh, 0, dumplo, sizeof kdh);
	if (i)
		printf("\nDump failed writing header (%d)\n", i);
	dumplo += sizeof kdh;
	i = 0;
	addr = 0;
	va = 0;
	mb = 0;
	for (count = 0; count < Maxmem;) {
		left = Maxmem - count;
		if (left > MAXDUMPPGS)
			left = MAXDUMPPGS;
		for (u = 0; u < left; u++) {
			a = addr + u * PAGE_SIZE;
			if (!is_physical_memory(a))
				a = 0;
			va = pmap_kenter_temporary(trunc_page(a), u);
		}
		i = count / (16*1024*1024 / PAGE_SIZE);
		if (i != mb) {
			printf(" %d", count / (1024 * 1024 / PAGE_SIZE));
			mb = i;
		}
		i = di->dumper(di->priv, va, 0, dumplo, left * PAGE_SIZE);
		if (i)
			break;
		count += left;
		dumplo += left * PAGE_SIZE;
		addr += left * PAGE_SIZE;
		if ((c = cncheckc()) == 0x03) {
			printf("\nDump aborted.\n");
			return;
		} else if (c != -1)
			printf("[CTRL-C to abort] ");
	}
	if (i) 
		printf("\nDump failed writing data (%d)\n", i);
	i = di->dumper(di->priv, &kdh, 0, dumplo, sizeof kdh);
	if (i)
		printf("\nDump failed writing trailer (%d)\n", i);
	di->dumper(di->priv, NULL, 0, 0, 0);  /* tell them we are done */
	printf("\nDump complete\n");
	return;
}
