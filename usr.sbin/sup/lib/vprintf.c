/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*
 * varargs versions of printf routines
 *
 **********************************************************************
 * HISTORY
 * $Log: vprintf.c,v $
 * Revision 1.1.1.1  1995/12/26 04:54:47  peter
 * Import the unmodified version of the sup that we are using.
 * The heritage of this version is not clear.  It appears to be NetBSD
 * derived from some time ago.
 *
 * Revision 1.1.1.1  1993/08/21  00:46:35  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:19  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 2.5  89/09/08  18:15:55  mbj
 * 	Use _doprnt() for the Multimax (an "old" architecture).
 * 	[89/09/08            mbj]
 * 
 * Revision 2.4  89/08/03  14:40:10  mja
 * 	Add vsnprintf() routine.
 * 	[89/07/12            mja]
 * 
 * 	Terminate vsprintf() string with null byte.
 * 	[89/04/21            mja]
 * 
 * 	Change to use new hidden name for _doprnt on MIPS.
 * 	[89/04/18            mja]
 * 
 * Revision 2.3  89/06/10  14:13:43  gm0w
 * 	Added putc of NULL byte to vsprintf.
 * 	[89/06/10            gm0w]
 * 
 * Revision 2.2  88/12/13  13:53:17  gm0w
 * 	From Brad White.
 * 	[88/12/13            gm0w]
 ************************************************************
 */

#include <stdio.h>
#include <varargs.h>

#ifdef DOPRINT_VA
/* 
 *  system provides _doprnt_va routine
 */
#define	_doprnt	_doprnt_va
#else
/*
 * system provides _doprnt routine
 */
#define _doprnt_va _doprnt
#endif


#ifdef NEED_VPRINTF
int
vprintf(fmt, args)
	char *fmt;
	va_list args;
{
	_doprnt(fmt, args, stdout);
	return (ferror(stdout) ? EOF : 0);
}

int
vfprintf(f, fmt, args)
	FILE *f;
	char *fmt;
	va_list args;
{
	_doprnt(fmt, args, f);
	return (ferror(f) ? EOF : 0);
}

int
vsprintf(s, fmt, args)
	char *s, *fmt;
	va_list args;
{
	FILE fakebuf;

	fakebuf._flag = _IOSTRG+_IOWRT;	/* no _IOWRT: avoid stdio bug */
	fakebuf._ptr = s;
	fakebuf._cnt = 32767;
	_doprnt(fmt, args, &fakebuf);
	putc('\0', &fakebuf);
	return (strlen(s));
}
#endif	/* NEED_VPRINTF */

#if	defined(NEED_VSNPRINTF) || defined(NEED_VPRINTF)
int
vsnprintf(s, n, fmt, args)
	char *s, *fmt;
	va_list args;
{
	FILE fakebuf;

#ifdef __hpux
	fakebuf._flag = _IODUMMY+_IOWRT;/* no _IOWRT: avoid stdio bug */
	fakebuf._base = fakebuf._ptr = s;
	fakebuf._cnt = n-1;
	fakebuf.__fileL = fakebuf.__fileH = 0xff;
#else
	fakebuf._flag = _IOSTRG+_IOWRT;	/* no _IOWRT: avoid stdio bug */
	fakebuf._ptr = s;
	fakebuf._cnt = n-1;
#endif
	_doprnt(fmt, args, &fakebuf);
	fakebuf._cnt++;
	putc('\0', &fakebuf);
	if (fakebuf._cnt<0)
	    fakebuf._cnt = 0;
	return (n-fakebuf._cnt-1);
}
#endif	/* NEED_VPRINTF || NEED_VSNPRINTF */
