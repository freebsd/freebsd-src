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
 *	BSDI exe.c,v 2.2 1996/04/08 19:32:34 bostic Exp
 * $Id: exe.c,v 1.3 1996/09/22 06:26:01 miff Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include "doscmd.h"

/* exports */
int	pspseg;
int	curpsp = 0;

/* locals */
static int		psp_s[10] = { 0 };
static int		env_s[10];
static regcontext_t	frames[10];
static char		*env_block;

static int
make_environment (char *cmdname, char **env)
{
    int i;
    int total;
    int len;
    int envseg;
    char *p;
    char *env_block;

    total = 0;
    for (i = 0; env[i]; i++) {
	debug (D_EXEC,"env: %s\n", env[i]);
	len = strlen (env[i]);
	if (total + len >= 32 * 1024)
	    break;
	total += len + 1;
    }

    total++; /* terminating null */
    total += 2; /* word count */
    total += strlen (cmdname) + 1;
    total += 4; /* some more zeros, just in case */

    if ((envseg = mem_alloc(total/16 + 1, 1, NULL)) == 0)
	fatal("out of memory for env\n");

    env_block = (char *)MAKEPTR(envseg, 0);
    memset (env_block, 0, total);

    p = env_block;
    total = 0;
    for (i = 0; env[i]; i++) {
	len = strlen (env[i]);
	if (total + len >= 32 * 1024)
	    break;
	total += len + 1;
	strcpy (p, env[i]);
	p += strlen (p) + 1;
    }	
    *p++ = 0;
    *(short *)p = strlen(cmdname);
    p += 2;
    strcpy (p, cmdname);
    while(*p) {
	if (*p == '/')
	    *p = '\\';
	else if (islower(*p))
	    *p = toupper(*p);
	p++;
    }
    *p = '\0';
    return(envseg);
}

static void
load_com(int fd, int start_segment)
{
    char *start_addr;
    int i;

    start_addr = (char *)MAKEPTR(start_segment, 0);

    lseek (fd, 0, 0);
    i = read (fd, start_addr, 0xff00);

    debug(D_EXEC, "Read %05x into %04x\n",
	  i, start_segment);
}

static void
load_exe(int fd, int start_segment, int reloc_segment, struct exehdr *hdr, int text_size)
{
    char *start_addr;
    int reloc_size;
    struct reloc_entry *reloc_tbl, *rp;
    u_short *segp;
    int i;

    start_addr = (char *)MAKEPTR(start_segment, 0);

    lseek (fd, hdr->hdr_size * 16, 0);
    if (read (fd, start_addr, text_size) != text_size)
	fatal ("error reading program text\n");
    debug(D_EXEC, "Read %05x into %04x\n",
	  text_size, start_segment);

    if (hdr->nreloc) {
	reloc_size = hdr->nreloc * sizeof (struct reloc_entry);

	if ((reloc_tbl = (struct reloc_entry *)malloc (reloc_size)) == NULL)
	    fatal ("out of memory for program\n");

	lseek (fd, hdr->reloc_offset, 0);
	if (read (fd, reloc_tbl, reloc_size) != reloc_size)
	    fatal ("error reading reloc table\n");

	for (i = 0, rp = reloc_tbl; i < hdr->nreloc; i++, rp++) {
	    segp = (u_short *)MAKEPTR(start_segment + rp->seg, rp->off);
	    *segp += start_segment;
	}
	free((char *)reloc_tbl);
    }
}

void
load_command(regcontext_t *REGS, int run, int fd, char *cmdname, 
	     u_short *param, char **argv, char **envs)
{
    struct exehdr hdr;
    int min_memory, max_memory;
    int biggest;
    int envseg;
    char *psp;
    int text_size;
    int i;
    int start_segment;
    int exe_file;
    char *p;
    int used, n;
    char *fcb;
    int newpsp;
    u_short init_cs, init_ip, init_ss, init_sp, init_ds, init_es;

    if (envs)
	envseg = make_environment(cmdname, envs);
    else
	envseg = env_s[curpsp];

    /* read exe header */
    if (read (fd, &hdr, sizeof hdr) != sizeof hdr)
	fatal ("can't read header\n");
    
    /* proper header ? */
    if (hdr.magic == 0x5a4d) {
	exe_file = 1;
	text_size = (hdr.size - 1) * 512 + hdr.bytes_on_last_page
	    - hdr.hdr_size * 16;
	min_memory = hdr.min_memory + (text_size + 15)/16;
	max_memory = hdr.max_memory + (text_size + 15)/16;
    } else {
	exe_file = 0;
	min_memory = 64 * (1024/16);
	max_memory = 0xffff;
    }
    
    /* alloc mem block */
    pspseg = mem_alloc(max_memory, 1, &biggest);
    if (pspseg == 0) {
	if (biggest < min_memory ||
	    (pspseg = mem_alloc(biggest, 1, NULL)) == 0)
	    fatal("not enough memory: needed %d have %d\n",
		  min_memory, biggest);
	
	max_memory = biggest;
    }
    
    mem_change_owner(pspseg, pspseg);
    mem_change_owner(envseg, pspseg);
    
    /* create psp */
    newpsp = curpsp + 1;
    psp_s[newpsp] = pspseg;
    env_s[newpsp] = envseg;
    
    psp = (char *)MAKEPTR(pspseg, 0);
    memset(psp, 0, 256);
    
    psp[0] = 0xcd;
    psp[1] = 0x20;

    *(u_short *)&psp[2] = pspseg + max_memory;
    
    /*
     * this is supposed to be a long call to dos ... try to fake it
     */
    psp[5] = 0xcd;
    psp[6] = 0x99;
    psp[7] = 0xc3;
    
    *(u_short *)&psp[0x16] = psp_s[curpsp];
    psp[0x18] = 1;
    psp[0x19] = 1;
    psp[0x1a] = 1;
    psp[0x1b] = 0;
    psp[0x1c] = 2;
    memset(psp + 0x1d, 0xff, 15);
    
    *(u_short *)&psp[0x2c] = envseg;
    
    *(u_short *)&psp[0x32] = 20;
    *(u_long *)&psp[0x34] = MAKEVEC(pspseg, 0x18);
    *(u_long *)&psp[0x38] = 0xffffffff;
    
    psp[0x50] = 0xcd;
    psp[0x51] = 0x98;
    psp[0x52] = 0xc3;
    
    p = psp + 0x81;
    *p = 0;
    used = 0;
    for (i = 0; argv[i]; i++) {
	n = strlen(argv[i]);
	if (used + 1 + n > 0x7d)
	    break;
	*p++ = ' ';
	memcpy(p, argv[i], n);
	p += n;
	used += n;
    }

    psp[0x80] = strlen(psp + 0x81);
    psp[0x81 + psp[0x80]] = 0x0d;
    psp[0x82 + psp[0x80]] = 0;
    
    p = psp + 0x81;
    parse_filename(0x00, p, psp + 0x5c, &n);
    p += n;
    parse_filename(0x00, p, psp + 0x6c, &n);
    
    if (param[4]) {
	fcb = (char *)MAKEPTR(param[4], param[3]);
	memcpy(psp + 0x5c, fcb, 16);
    }
    if (param[6]) {
	fcb = (char *)MAKEPTR(param[6], param[5]);
	memcpy(psp + 0x6c, fcb, 16);
    }

#if 0
    printf("005c:");
    for (n = 0; n < 16; n++)
	printf(" %02x", psp[0x5c + n]);
    printf("\n");
    printf("006c:");
    for (n = 0; n < 16; n++)
	printf(" %02x", psp[0x6c + n]);
    printf("\n");
#endif

    disk_transfer_addr = MAKEVEC(pspseg, 0x80);
    
    start_segment = pspseg + 0x10;
    
    if (!exe_file) {
	load_com(fd, start_segment);

	init_cs = pspseg;
	init_ip = 0x100;
	init_ss = init_cs;
	init_sp = 0xfffe;
	init_ds = init_cs;
	init_es = init_cs;
    } else {
	load_exe(fd, start_segment, start_segment, &hdr, text_size);
	
	init_cs = hdr.init_cs + start_segment;
	init_ip = hdr.init_ip;
	init_ss = hdr.init_ss + start_segment;
	init_sp = hdr.init_sp;
	init_ds = pspseg;
	init_es = init_ds;
    }

    debug(D_EXEC, "cs:ip = %04x:%04x, ss:sp = %04x:%04x, "
	  "ds = %04x, es = %04x\n",
	  init_cs, init_ip, init_ss, init_sp, init_ds, init_es);
    
    if (run) {
	frames[newpsp] = *REGS;
	curpsp = newpsp;
	
	R_EFLAGS = 0x20202;
	R_CS = init_cs;
	R_IP = init_ip;
	R_SS = init_ss;
	R_SP = init_sp;
	R_DS = init_ds;
	R_ES = init_es;

	R_AX = R_BX = R_CX = R_DX = R_SI = R_DI = R_BP = 0;

    } else {
	param[7] = init_sp;
	param[8] = init_ss;
	param[9] = init_ip;
	param[10] = init_cs;
    }
}

void
load_overlay(int fd, int start_segment, int reloc_segment)
{
    struct exehdr hdr;
    int text_size;
    int exe_file;

    /* read exe header */
    if (read (fd, &hdr, sizeof hdr) != sizeof hdr)
	fatal ("can't read header\n");
    
    /* proper header ? */
    if (hdr.magic == 0x5a4d) {
	exe_file = 1;
	text_size = (hdr.size - 1) * 512 + hdr.bytes_on_last_page
	    - hdr.hdr_size * 16;
    } else {
	exe_file = 0;
    }

    if (!exe_file)
	load_com(fd, start_segment);
    else
	load_exe(fd, start_segment, reloc_segment, &hdr, text_size);
}

static int
get_psp(void)
{
    return(psp_s[curpsp]);
}

int
get_env(void)
{
    return(env_s[curpsp]);
}

void
exec_command(regcontext_t *REGS, int run,
	     int fd, char *cmdname, u_short *param)
{
    char *arg;
    char *env;
    char *argv[2];
    char *envs[100];

    env = (char *)MAKEPTR(param[0], 0);
    arg = (char *)MAKEPTR(param[2], param[1]);

    if (arg) {
	int nbytes = *arg++;
	arg[nbytes] = 0;
	if (!*arg)
	    arg = NULL;
    }
    argv[0] = arg;
    argv[1] = NULL;

    debug (D_EXEC, "exec_command: cmdname = %s\n"
		   "env = 0x0%x, arg = %04x:%04x(%s)\n",
    	cmdname, param[0], param[2], param[1], arg);

    if (env) {
	int i;
	for ( i=0; i < 99 && *env; ++i ) {
	    envs[i] = env;
	    env += strlen(env)+1;
	}
	envs[i] = NULL;
	load_command(REGS, run, fd, cmdname, param, argv, envs);
    } else
	load_command(REGS, run, fd, cmdname, param, argv, NULL);
}

void
exec_return(regcontext_t *REGS, int code)
{
    debug(D_EXEC, "Returning from exec\n");
    mem_free_owner(psp_s[curpsp]);
    *REGS = frames[curpsp--];
    R_AX = code;
    R_FLAGS &= ~PSL_C;		/* It must have worked */
}
