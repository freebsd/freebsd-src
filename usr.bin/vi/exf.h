/*-
 * Copyright (c) 1992, 1993, 1994
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
 *
 *	@(#)exf.h	8.23 (Berkeley) 3/23/94
 */
					/* Undo direction. */
/*
 * exf --
 *	The file structure.
 */
struct _exf {
	int	 refcnt;		/* Reference count. */

					/* Underlying database state. */
	DB	*db;			/* File db structure. */
	char	*c_lp;			/* Cached line. */
	size_t	 c_len;			/* Cached line length. */
	recno_t	 c_lno;			/* Cached line number. */
	recno_t	 c_nlines;		/* Cached lines in the file. */

	DB	*log;			/* Log db structure. */
	char	*l_lp;			/* Log buffer. */
	size_t	 l_len;			/* Log buffer length. */
	recno_t	 l_high;		/* Log last + 1 record number. */
	recno_t	 l_cur;			/* Log current record number. */
	MARK	 l_cursor;		/* Log cursor position. */
	enum direction lundo;		/* Last undo direction. */

	LIST_HEAD(_markh, _lmark) marks;/* Linked list of file MARK's. */

	/*
	 * Paths for the recovery mail file and the vi recovery file and
	 * a file descriptor for the former.  We keep a file descriptor
	 * to the recovery file open and locked, while the file is in use.
	 * This allows the recovery option to distinguish between files
	 * that are live, and those that should be recovered.
	 *
	 * F_RCV_ON is set as long as we believe that the file is recoverable.
	 * This doesn't mean that any initialization has been done, however.
	 * If F_RCV_NORM is not set and rcv_path and rcv_mpath are not NULL,
	 * they are unlinked on file exit.  If not NULL they are free'd on file
	 * exit.  On file exit, if rcv_fd is not -1, it is closed.
	 */
#define	RCV_PERIOD	120		/* Sync every two minutes. */
	char	*rcv_path;		/* Recover file name. */
	char	*rcv_mpath;		/* Recover mail file name. */
	int	 rcv_fd;		/* Locked mail file descriptor. */
	struct timeval rcv_tod;		/* ITIMER_REAL: recovery time-of-day. */

#define	F_FIRSTMODIFY	0x001		/* File not yet modified. */
#define	F_MODIFIED	0x002		/* File is currently dirty. */
#define	F_MULTILOCK	0x004		/* Multiple processes running, lock. */
#define	F_NOLOG		0x008		/* Logging turned off. */
#define	F_RCV_ALRM	0x010		/* File needs to be synced. */
#define	F_RCV_NORM	0x020		/* Don't delete recovery files. */
#define	F_RCV_ON	0x040		/* Recovery is possible. */
#define	F_UNDO		0x080		/* No change since last undo. */
	u_int	 flags;
};

/* Flags to file_write(). */
#define	FS_ALL		0x01		/* Write the entire file. */
#define	FS_APPEND	0x02		/* Append to the file. */
#define	FS_FORCE	0x04		/* Force is set. */
#define	FS_POSSIBLE	0x08		/* Force could be set. */

#define	GETLINE_ERR(sp, lno) {						\
	msgq((sp), M_ERR,						\
	    "Error: %s/%d: unable to retrieve line %u.",		\
	    tail(__FILE__), __LINE__, (lno));				\
}

/* FREF routines. */
FREF	*file_add __P((SCR *, FREF *, CHAR_T *, int));
FREF	*file_first __P((SCR *));
FREF	*file_next __P((SCR *, FREF *));
FREF	*file_prev __P((SCR *, FREF *));
FREF	*file_unedited __P((SCR *));

/* EXF routines. */
int	 file_end __P((SCR *, EXF *, int));
int	 file_init __P((SCR *, FREF *, char *, int));
int	 file_write __P((SCR *, EXF *, MARK *, MARK *, char *, int));

/* Recovery routines. */
void	 rcv_hup __P((void));
int	 rcv_init __P((SCR *, EXF *));
int	 rcv_list __P((SCR *));
int	 rcv_read __P((SCR *, char *));
int	 rcv_sync __P((SCR *, EXF *));
void	 rcv_term __P((void));
int	 rcv_tmp __P((SCR *, EXF *, char *));

/* DB interface routines */
int	 file_aline __P((SCR *, EXF *, int, recno_t, char *, size_t));
int	 file_dline __P((SCR *, EXF *, recno_t));
char	*file_gline __P((SCR *, EXF *, recno_t, size_t *));
int	 file_iline __P((SCR *, EXF *, recno_t, char *, size_t));
int	 file_lline __P((SCR *, EXF *, recno_t *));
char	*file_rline __P((SCR *, EXF *, recno_t, size_t *));
int	 file_sline __P((SCR *, EXF *, recno_t, char *, size_t));
