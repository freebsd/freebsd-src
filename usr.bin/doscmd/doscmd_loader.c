/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI doscmd_loader.c,v 2.3 1996/04/08 19:32:33 bostic Exp
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <a.out.h>

/*
 * reserve space in "low" memory for the interrupt vector table
 */
static const char filler[4096] = { 0, };

#define _PATH_DOS_KERNEL_DIR	"/usr/libexec/"
#define _PATH_DOS_KERNEL	"doscmd.kernel"

int
load_kernel(void)
{
    FILE *fp;
    struct exec exec;
    int start_address;

    if ((fp = fopen(_PATH_DOS_KERNEL, "r")) == NULL &&
	(fp = fopen("obj/" _PATH_DOS_KERNEL, "r")) == NULL &&
	(fp = fopen(_PATH_DOS_KERNEL_DIR _PATH_DOS_KERNEL, "r")) == NULL &&
	(fp = fopen(getenv("DOS_KERNEL"), "r")) == NULL)
	err(1, "load_kernel");

    if (fread(&exec, sizeof(exec), 1, fp) != 1 || N_GETMAGIC(exec) != OMAGIC)
	errx(1, "bad kernel file format");

    start_address = exec.a_entry & (~(getpagesize() - 1));
    if (brk(start_address + exec.a_text + exec.a_data + exec.a_bss) < 0)
	err(1, "load_kernel");
    fread((char *)start_address, exec.a_text + exec.a_data, 1, fp);
    bzero((char *)(start_address + exec.a_text + exec.a_data), exec.a_bss);
    fclose(fp);
    return(exec.a_entry);
}

void
main(int argc, char **argv, char **environ)
{
    void (*entry_point)();
#ifndef __FreeBSD__
    int fd = open("/dev/mem", 0);
#endif
    setgid(getgid());
    setuid(getuid());

#ifndef __FreeBSD__
    if (fd < 0)
	err(1, "/dev/mem");
#endif

    entry_point = (void (*)()) load_kernel();

#ifndef __FreeBSD__
    if (read(fd, 0, 0x500 != 0x500))
	err(1, "/dev/mem");

    close(fd);
#endif

    (*entry_point)(argc, argv, environ);
    errx(1, "return from doscmd kernel???");
}
