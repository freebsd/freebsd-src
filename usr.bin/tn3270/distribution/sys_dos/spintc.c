/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)spintc.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <stdio.h>
#include <dos.h>
#include <stdlib.h>

#include "../general/general.h"
#include "spint.h"

#define	PSP_ENVIRONMENT		0x2c
#define	PSP_FCB1		0x5c
#define	PSP_FCB2		0x6c

typedef struct {
    int
	environment,		/* Segment address of environment */
	cmd_ptr_offset,		/* Offset of command to execute */
	cmd_ptr_segment,	/* Segment where command lives */
	fcb1_ptr_offset,	/* Offset of FCB 1 */
	fcb1_ptr_segment,	/* Segment of FCB 1 */
	fcb2_ptr_offset,	/* Offset of FCB 2 */
	fcb2_ptr_segment;	/* Segment of FCB 2 */
} ExecList;


static int int_offset, int_segment;


void
spint_finish(spint)
Spint *spint;
{
    union REGS regs;
    struct SREGS sregs;

    if (spint->done == 0) {
	return;				/* Not done yet */
    }

    /*
     * Restore old interrupt handler.
     */

    regs.h.ah = 0x25;
    regs.h.al = spint->int_no;
    regs.x.dx = int_offset;
    sregs.ds = int_segment;
    intdosx(&regs, &regs, &sregs);

    if (spint->regs.x.cflag) {
	fprintf(stderr, "0x%x return code from EXEC.\n", spint->regs.x.ax);
	spint->done = 1;
	spint->rc = 99;
	return;
    }

    regs.h.ah = 0x4d;			/* Get return code */

    intdos(&regs, &regs);

    spint->rc = regs.x.ax;
}

void
spint_continue(spint)
Spint *spint;
{
    _spint_continue(spint);		/* Return to caller */
    spint_finish(spint);
}


void
spint_start(command, spint)
char *command;
Spint *spint;
{
    ExecList mylist;
    char *comspec;
    void _spint_int();
    union REGS regs;
    struct SREGS sregs;

    /*
     * Get comspec.
     */
    comspec = getenv("COMSPEC");
    if (comspec == 0) {			/* Can't find where command.com is */
	fprintf(stderr, "Unable to find COMSPEC in the environment.");
	spint->done = 1;
	spint->rc = 99;	/* XXX */
	return;
    }

    /*
     * Now, hook up our interrupt routine.
     */

    regs.h.ah = 0x35;
    regs.h.al = spint->int_no;
    intdosx(&regs, &regs, &sregs);

    /* Save old routine */
    int_offset = regs.x.bx;
    int_segment = sregs.es;

    regs.h.ah = 0x25;
    regs.h.al = spint->int_no;
    regs.x.dx = (int) _spint_int;
    segread(&sregs);
    sregs.ds = sregs.cs;
    intdosx(&regs, &regs, &sregs);

    /*
     * Read in segment registers.
     */

    segread(&spint->sregs);

    /*
     * Set up registers for the EXEC call.
     */

    spint->regs.h.ah = 0x4b;
    spint->regs.h.al = 0;
    spint->regs.x.dx = (int) comspec;
    spint->sregs.es = spint->sregs.ds;		/* Superfluous, probably */
    spint->regs.x.bx = (int) &mylist;

    /*
     * Set up EXEC parameter list.
     */

    ClearElement(mylist);
    mylist.cmd_ptr_offset = (int) command;
    mylist.cmd_ptr_segment = spint->sregs.ds;
    mylist.fcb1_ptr_offset = PSP_FCB1;
    mylist.fcb1_ptr_segment = _psp;
    mylist.fcb2_ptr_offset = PSP_FCB2;
    mylist.fcb2_ptr_segment = _psp;
    mylist.environment = *((int far *)(((long)_psp<<16)|PSP_ENVIRONMENT));

    /*
     * Call to assembly language routine to actually set up for
     * the spint.
     */

    _spint_start(spint);

    spint_finish(spint);
}
