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
 * $Id: modem.h,v 1.8 1997/06/09 03:27:30 brian Exp $
 *
 *	TODO:
 */

#ifndef _MODEM_H_
#define	_MODEM_H_
#include <termios.h>
#include "mbuf.h"

extern int RawModem(int);
extern void UnrawModem(int);
extern void UpModem(int);
extern void DownModem(int);
extern void WriteModem(int, char *, int);
extern void ModemStartOutput(int);
extern int OpenModem(int);
extern void CloseModem(void);
extern int ModemSpeed(void);
extern int ModemQlen(void);
extern int DialModem(void);
extern int SpeedToInt(speed_t);
extern speed_t IntToSpeed(int);
extern void ModemTimeout(void);
extern void DownConnection(void);
extern void ModemOutput(int, struct mbuf *);

#endif
