/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: modem.h,v 1.2 1995/02/26 12:17:46 amurai Exp $
 *
 *	TODO:
 */

#ifndef _MODEM_H_
#define	_MODEM_H_
#include <termios.h>
#include "mbuf.h"
#include <sys/cdefs.h>

extern int RawModem __P((int));
extern void UnrawModem __P((int));
extern void UpModem __P((int));
extern void DownModem __P((int));
extern void WriteModem __P((int, char *, int));
extern void ModemStartOutput __P((int));
extern int OpenModem __P((int));
extern int ModemSpeed __P((void));
extern int ModemQlen __P((void));
extern int DialModem __P((void));

#endif
