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
 *	BSDI int17.c,v 2.2 1996/04/08 19:32:48 bostic Exp
 *
 * $Id: int17.c,v 1.2 1998/01/22 02:44:54 msmith Exp $
 */

#include "doscmd.h"
#include <signal.h>

static int	lpt_fd[4] = { -1, -1, -1, -1, };
static FILE	*lpt_file[4] = { 0, 0, 0, 0};
static int	direct[4] = { 0, 0, 0, 0};
static char	*queue[4] = { 0, 0, 0, 0};
static int	timeout[4] = { 30, 30, 30, 30 };
static int	last_poll[4] = { 0, 0, 0, 0};
static int	last_count[4] = { 0, 0, 0, 0};
static int	current_count[4] = { 0, 0, 0, 0};
static int	alarm_active[4] = { 0, 0, 0, 0};
static int	alarm_set = 0;

static void open_printer(int printer);

void
int17(regcontext_t *REGS)
{
    char	printer_name[20];
    int	fd;
    int p;
    u_char c;

    switch (R_AH) {

    case 0x00:
	reset_poll();
	
	fd = lpt_fd[R_DX];
	if (fd == -1) {
	    open_printer(R_DX);
	    fd = lpt_fd[R_DX];
	}
	if (fd >= 0) {
	    c = R_AL;
	    write(fd, &c, 1);
	}
	R_AH = 0x90;		/* printed selected */
	current_count[R_DX]++;
	break;

    case 0x01:
    case 0x02:
	R_AH = 0x90;
	break;

    default:
	unknown_int2(0x17, R_AH, REGS);
	break;
    }
}

void 
lpt_poll(void)
{
    int	i;
    int	current;

    current = time(0);

    for(i=0; i < 4; i++) {
	if (lpt_fd[i] < 0)
	    continue;

	if (current - last_poll[i] < timeout[i])
	    continue;

	last_poll[i] = current;

	if (last_count[i] == current_count[i]) {
	    if (direct[i]) {
		debug(D_PRINTER, "Closing printer %d\n", i);
		close(lpt_fd[i]);
	    } else {
		debug(D_PRINTER, "Closing spool printer %d\n", i);
		pclose(lpt_file[i]);
	    }
	    lpt_fd[i] = -1;
	    lpt_file[i] = 0;
	}

	last_count[i] = current_count[i];
    }
}


static void
open_printer(int printer)
{
    char	printer_name[80];
    char	command[120];
    int		fd;
    FILE	*file;
    char	*p;

    /*
     * if printer is direct then open output device.
     */
    if (direct[printer]) {
	if (p = queue[printer]) {
	    if ((fd = open(p, O_WRONLY|O_APPEND|O_CREAT, 0666)) < 0) {
		perror(p);
		return;
	    }
	} else {
	    sprintf(printer_name, "/dev/lpt%d", printer);
	    debug(D_PRINTER, "Opening device %s\n", printer_name);
	    if ((fd = open(printer_name, O_WRONLY)) < 0) {
		perror(printer_name);
		return;
	    }
    	}
	lpt_fd[printer] = fd;
	return;
    }

    /*
     * If printer is a spooled device then open pipe to spooled device
     */
    if (queue[printer]) {
	strncpy(printer_name, queue[printer], sizeof(printer_name));
	printer_name[sizeof(printer_name) - 1] = '\0';
    } else
	strcpy(printer_name, "lp");

    snprintf(command, sizeof(command), "lpr -P %s", printer_name);
    debug(D_PRINTER, "opening pipe to %s\n", printer_name);

    if ((file = popen(command, "w")) == 0) {
	perror(command);
	return;
    }
    lpt_file[printer] = file;
    lpt_fd[printer] = fileno(file);
}

void
printer_direct(int printer)
{
    direct[printer] = 1;
}

void
printer_spool(int printer, char *print_queue)
{
    queue[printer] = print_queue ? strdup(print_queue) : 0;
}

void
printer_timeout(int printer, char *time_out)
{
    if (atoi(time_out) <= 0) {
	fprintf(stderr, "Bad timeout value on lpt%d:\n", printer+1);
	quit(1);
    }
    timeout[printer] = atoi(time_out);
}
