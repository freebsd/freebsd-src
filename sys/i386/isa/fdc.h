/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *
 *	from:	@(#)fd.c	7.4 (Berkeley) 5/25/91
 *	$FreeBSD$
 *
 */

enum fdc_type
{
	FDC_NE765, FDC_I82077, FDC_NE72065, FDC_UNKNOWN = -1
};


/***********************************************************************\
* Per controller structure.						*
\***********************************************************************/
struct fdc_data
{
	int	fdcu;		/* our unit number */
	int	baseport;
	int	dmachan;
	int	flags;
#define FDC_ATTACHED	0x01
#define FDC_HASFTAPE	0x02
#define FDC_TAPE_BUSY	0x04
#define FDC_STAT_VALID	0x08
	struct	fd_data *fd;
	int	fdu;		/* the active drive	*/
	int	state;
	int	retry;
	int	fdout;		/* mirror of the w/o digital output reg */
	u_long	status[7];	/* copy of the registers */
	enum	fdc_type fdct;	/* chip version of FDC */
	int	fdc_errs;	/* number of logged errors */
	struct buf_queue_head head;	/* Head of buf chain	  */
};

/***********************************************************************\
* Throughout this file the following conventions will be used:		*
* fd is a pointer to the fd_data struct for the drive in question	*
* fdc is a pointer to the fdc_data struct for the controller		*
* fdu is the floppy drive unit number					*
* fdcu is the floppy controller unit number				*
* fdsu is the floppy drive unit number on that controller. (sub-unit)	*
\***********************************************************************/
typedef int	fdu_t;
typedef int	fdcu_t;
typedef int	fdsu_t;
typedef	struct fd_data *fd_p;
typedef struct fdc_data *fdc_p;
typedef enum fdc_type fdc_t;

#define FDUNIT(s)	(((s)>>6)&03)
#define FDTYPE(s)	((s)&077)
