/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * Copyright (c) 1992-1998 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id:$
 */

#include "sc.h"
#include "opt_syscons.h"

#if NSC > 0

#ifndef SC_NO_HISTORY

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/console.h>

#include <dev/syscons/syscons.h>

#if !defined(SC_MAX_HISTORY_SIZE)
#define SC_MAX_HISTORY_SIZE	(1000 * MAXCONS * NSC)
#endif

#if !defined(SC_HISTORY_SIZE)
#define SC_HISTORY_SIZE		(ROW * 4)
#endif

#if (SC_HISTORY_SIZE * MAXCONS * NSC) > SC_MAX_HISTORY_SIZE
#undef SC_MAX_HISTORY_SIZE
#define SC_MAX_HISTORY_SIZE	(SC_HISTORY_SIZE * MAXCONS * NSC)
#endif

/* local variables */
static int		extra_history_size
				= SC_MAX_HISTORY_SIZE - SC_HISTORY_SIZE*MAXCONS;

/* local functions */
static void history_to_screen(scr_stat *scp);

/* allocate a history buffer */
int
sc_alloc_history_buffer(scr_stat *scp, int lines, int wait)
{
	/*
	 * syscons unconditionally allocates buffers upto 
	 * SC_HISTORY_SIZE lines or scp->ysize lines, whichever 
	 * is larger. A value greater than that is allowed, 
	 * subject to extra_history_size.
	 */
	sc_vtb_t *history;
	int cur_lines;				/* current buffer size */
	int min_lines;				/* guaranteed buffer size */

	if (lines <= 0)
		lines = SC_HISTORY_SIZE;	/* use the default value */
	lines = imax(lines, scp->ysize);
	min_lines = imax(SC_HISTORY_SIZE, scp->ysize);

	history = scp->history;
	scp->history = NULL;
	if (history == NULL) {
		cur_lines = 0;
	} else {
		cur_lines = sc_vtb_rows(history);
		if (cur_lines > min_lines)
			extra_history_size += cur_lines - min_lines;
		sc_vtb_destroy(history);
	}

	if (lines > min_lines)
		extra_history_size -= lines - min_lines;
	history = (sc_vtb_t *)malloc(sizeof(*history),
				     M_DEVBUF, (wait) ? M_WAITOK : M_NOWAIT);
	if (history != NULL)
		sc_vtb_init(history, VTB_RINGBUFFER, scp->xsize, lines,
			    NULL, wait);
	scp->history_pos = 0;
	scp->history = history;

	return 0;
}

/* copy entire screen into the top of the history buffer */
void
sc_hist_save(scr_stat *scp)
{
	sc_vtb_append(&scp->vtb, 0, scp->history, scp->xsize*scp->ysize);
	scp->history_pos = sc_vtb_tail(scp->history);
}

/* restore the screen by copying from the history buffer */
int
sc_hist_restore(scr_stat *scp)
{
	int ret;

	if (scp->history_pos != sc_vtb_tail(scp->history)) {
		scp->history_pos = sc_vtb_tail(scp->history);
		history_to_screen(scp);
		ret =  0;
	} else {
		ret = 1;
	}
	sc_vtb_seek(scp->history, sc_vtb_pos(scp->history, 
					     sc_vtb_tail(scp->history),
					     -scp->xsize*scp->ysize));
	return ret;
}

/* copy screen-full of saved lines */
static void
history_to_screen(scr_stat *scp)
{
	int pos;
	int i;

	pos = scp->history_pos;
	for (i = 1; i <= scp->ysize; ++i) {
		pos = sc_vtb_pos(scp->history, pos, -scp->xsize);
		sc_vtb_copy(scp->history, pos,
			    &scp->vtb, scp->xsize*(scp->ysize - i),
			    scp->xsize);
	}
	mark_all(scp);
}

/* go to the tail of the history buffer */
void
sc_hist_home(scr_stat *scp)
{
	scp->history_pos = sc_vtb_tail(scp->history);
	history_to_screen(scp);
}

/* go to the top of the history buffer */
void
sc_hist_end(scr_stat *scp)
{
	scp->history_pos = sc_vtb_pos(scp->history, sc_vtb_tail(scp->history),
				      scp->xsize*scp->ysize);
	history_to_screen(scp);
}

/* move one line up */
int
sc_hist_up_line(scr_stat *scp)
{
	if (sc_vtb_pos(scp->history, scp->history_pos, -(scp->xsize*scp->ysize))
	    == sc_vtb_tail(scp->history))
		return -1;
	scp->history_pos = sc_vtb_pos(scp->history, scp->history_pos,
				      -scp->xsize);
	history_to_screen(scp);
	return 0;
}

/* move one line down */
int
sc_hist_down_line(scr_stat *scp)
{
	if (scp->history_pos == sc_vtb_tail(scp->history))
		return -1;
	scp->history_pos = sc_vtb_pos(scp->history, scp->history_pos,
				      scp->xsize);
	history_to_screen(scp);
	return 0;
}

int
sc_hist_ioctl(struct tty *tp, u_long cmd, caddr_t data, int flag,
	      struct proc *p)
{
	scr_stat *scp;

	switch (cmd) {

	case CONS_HISTORY:  	/* set history size */
		scp = sc_get_scr_stat(tp->t_dev);
		if (*(int *)data <= 0)
			return EINVAL;
		if (scp->status & BUFFER_SAVED)
			return EBUSY;
		return sc_alloc_history_buffer(scp, 
					       imax(*(int *)data, scp->ysize),
					       TRUE);
	}

	return ENOIOCTL;
}

#endif /* SC_NO_HISTORY */

#endif /* NSC */
