/*-
 * Copyright (c) 1988 The Regents of the University of California.
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
 *	@(#)globals.h	4.2 (Berkeley) 4/26/91
 */

/*
 *	This file contains all the globals used by tn3270.
 *
 *	Since various files may want to reference this file,
 *	and since they may only want subsets of the globals,
 *	we assume they have #include'd all the other .h files
 *	first, and we only give those globals relevant to
 *	the #include'd .h files.
 *
 */

#if	defined(DEFINING_INSTANCES)
#define	EXTERN
#else
#define	EXTERN extern
#endif


EXTERN int
		/*
		 * shell_active ==>
		 *		1.  Don't do input.
		 *		2.  Don't do output.
		 *		3.  Don't block in select.
		 *		4.  When nothing to do, call shell_continue()
		 */
	shell_active;


#if	defined(INCLUDED_OPTIONS)
EXTERN int	OptHome;		/* where home should send us */

EXTERN int	OptLeftMargin;		/* where new line should send us */

EXTERN char	OptColTabs[80];		/* local tab stops */

EXTERN int	OptAPLmode;

EXTERN int	OptNullProcessing;	/* improved null processing */

EXTERN int	OptZonesMode;		/* zones mode off */

EXTERN int	OptEnterNL;		/* regular enter/new line keys */

EXTERN int	OptColFieldTab;		/* regular column/field tab keys */

EXTERN int	OptPacing;		/* do pacing */

EXTERN int	OptAlphaInNumeric;	/* allow alpha in numeric fields */

EXTERN int	OptHome;

EXTERN int	OptLeftMargin;

EXTERN int	OptWordWrap;
#endif

#if	defined(INCLUDED_SCREEN)
EXTERN ScreenImage
	Host[MAXSCREENSIZE];		/* host view of screen */

EXTERN char	Orders[256];			/* Non-zero for orders */

			/* Run-time screen geometry */
EXTERN int
	MaxNumberLines,		/* How many rows the 3270 COULD have */
	MaxNumberColumns,	/* How many columns the 3270 COULD have */
	NumberLines,		/* How many lines the 3270 screen contains */
	NumberColumns,		/* How many columns the 3270 screen contains */
	ScreenSize;

EXTERN int CursorAddress;			/* where cursor is */
EXTERN int BufferAddress;			/* where writes are going */

EXTERN int Lowest, Highest;

extern char CIABuffer[];

EXTERN int UnLocked;		/* is the keyboard unlocked */
EXTERN int AidByte;

#endif

#if	defined(INCLUDED_STATE)
#endif

#if	defined(INCLUDED_OIA)

EXTERN OIA OperatorInformationArea;

EXTERN int
    oia_modified,		/* Has the oia been modified */
    ps_modified;		/* Has the presentation space been modified */

#endif	/* defined(INCLUDED_OIA) */
